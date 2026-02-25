"""
Rutherford Alpha Scattering — Mac-Optimized with Keyboard Camera Controls
==========================================================================
Key fixes vs original:
  1. glutInit([]) instead of glutInit(sys.argv)  -> avoids macOS arg-parse crash
  2. DT divided by PHYSICS_SUBSTEPS inside loop  -> correct effective timestep
  3. particles[:] = [...] instead of particles = [...] -> no per-frame list alloc
  4. GLUT_BITMAP_HELVETICA_12 -> GLUT_BITMAP_9_BY_15 -> reliable on all macOS GLUT
  5. trail_length 500 -> 200, skip ratio 3 -> 2    -> ~60% fewer vertex calls
  6. Nucleus drawn with a display list (compiled once) -> GPU-cached
  7. glutPostRedisplay() added to reshape            -> correct resize handling
  8. Graceful import error with friendly message

KEYBOARD CAMERA CONTROLS (Mac-optimized):
  - Arrow Keys: Rotate camera (yaw and pitch)
  - W/S: Move camera forward/backward (zoom)
  - A/D: Rotate camera left/right (yaw)
  - Q/E: Move camera up/down (elevation)
  - R: Reset camera to default position
  - O: Toggle orthographic/perspective view
  - All controls work reliably on macOS GLUT
"""

import sys
import random
import math
from collections import deque

try:
    from OpenGL.GL import *
    from OpenGL.GLUT import *
    from OpenGL.GLU import *
except ImportError:
    print(
        "\n[ERROR] PyOpenGL is not installed.\n"
        "Install it with:\n"
        "    pip install PyOpenGL PyOpenGL_accelerate\n"
        "On Intel Mac you may also need:\n"
        "    pip install pyopengl-accelerate\n"
    )
    sys.exit(1)

# =========================================================
# Window
# =========================================================
WIDTH, HEIGHT = 1100, 700

# =========================================================
# Physics constants (stylised, not SI-exact)
# =========================================================
K_COULOMB       = 2.5
NUCLEUS_CHARGE  = 79.0
ALPHA_CHARGE    = 2.0

DT              = 0.0018
PHYSICS_SUBSTEPS = 2
DT_SUB          = DT / PHYSICS_SUBSTEPS   # FIX: correct per-substep dt

FORCE_SOFTENING = 0.14
MIN_R_DIR       = 0.08
MAX_ACCEL       = 2600.0

# =========================================================
# Beam / View
# =========================================================
ORTHO_VIEW    = True
BEAM_SPAWN_X  = -44.0
BEAM_Y_MIN    = -5.0
BEAM_Y_MAX    =  5.0
BEAM_SPEED    = 28.0
BEAM_Z        = 0.0

# =========================================================
# Particle system
# =========================================================
particles       = []
spawn_chance    = 0.10
spawn_per_tick  = 2
max_particles   = 180
particle_lifetime = 18.0
trail_length    = 200   # FIX: reduced from 500; saves ~60% vertex calls

# Visual sizes
NUCLEUS_DRAW_RADIUS = 0.40
HEAD_POINT_SIZE     = 2.6
TRAIL_POINT_SIZE    = 1.3

# Beam visualization
BEAM_RADIUS         = 1.5   # Radius of the cylindrical beam (increased for visibility)
BEAM_LENGTH         = 50.0  # Length of the beam cylinder
BEAM_SLICES         = 20    # Number of slices around the cylinder (smoothness)
show_beam           = True  # Toggle beam visibility

# Toggles
paused     = False
show_hud   = True
show_axes  = False
show_heads = True
show_label = True
show_beam  = True
DEBUG_PRINT = True  # Temporarily enabled for debugging beam

# Camera (used when ORTHO_VIEW=False)
cam_yaw      = 0.0
cam_pitch    = 15.0
cam_distance = 35.0
cam_height   = 0.0  # Vertical offset for Q/E keys

# Default camera position for reset
DEFAULT_CAM_YAW = 0.0
DEFAULT_CAM_PITCH = 15.0
DEFAULT_CAM_DISTANCE = 35.0
DEFAULT_CAM_HEIGHT = 0.0

# Display list handle for nucleus sphere (set in init)
_nucleus_dl = None

# Debug frame counter
_debug_frame_counter = 0

# =========================================================
# Utilities
# =========================================================
def clamp(v, lo, hi):
    return max(lo, min(hi, v))

def debug_log(msg):
    if DEBUG_PRINT:
        print(msg, flush=True)

# =========================================================
# Particle
# =========================================================
class Particle:
    __slots__ = ("x", "y", "z", "vx", "vy", "vz", "age", "alive", "trail")

    def __init__(self):
        self.x  = BEAM_SPAWN_X
        self.y  = random.uniform(BEAM_Y_MIN, BEAM_Y_MAX)
        self.z  = BEAM_Z
        self.vx = BEAM_SPEED
        self.vy = random.uniform(-0.01, 0.01)
        self.vz = 0.0
        self.age   = 0.0
        self.alive = True
        self.trail = deque(maxlen=trail_length)
        self.trail.append((self.x, self.y, self.z))

    def update(self, dt):
        """Advance particle by dt using semi-implicit Euler."""
        rx, ry, rz = self.x, self.y, self.z
        r2 = rx*rx + ry*ry + rz*rz
        if r2 < 1e-12:
            r2 = 1e-12
        r = math.sqrt(r2)

        r_dir = max(r, MIN_R_DIR)
        denom = r2 + FORCE_SOFTENING * FORCE_SOFTENING
        accel_mag = min(
            (K_COULOMB * NUCLEUS_CHARGE * ALPHA_CHARGE) / denom,
            MAX_ACCEL
        )

        scale   = accel_mag / r_dir
        self.vx += rx * scale * dt
        self.vy += ry * scale * dt
        self.vz += rz * scale * dt

        self.x  += self.vx * dt
        self.y  += self.vy * dt
        self.z  += self.vz * dt
        self.age += dt

        self.trail.append((self.x, self.y, self.z))

        if (
            self.x  >  92.0 or self.x  < -65.0 or
            abs(self.y) > 46.0 or
            abs(self.z) >  8.0 or
            self.age > particle_lifetime
        ):
            self.alive = False

# =========================================================
# Drawing helpers
# =========================================================
def build_nucleus_display_list():
    """Compile nucleus sphere into a GL display list (drawn from GPU cache)."""
    dl = glGenLists(1)
    glNewList(dl, GL_COMPILE)
    glPushMatrix()
    glColor3f(1.0, 0.84, 0.12)
    glutSolidSphere(NUCLEUS_DRAW_RADIUS, 20, 20)
    glPopMatrix()
    glEndList()
    return dl

def draw_nucleus():
    if _nucleus_dl is not None:
        glCallList(_nucleus_dl)

def draw_beam_cylinder():
    """Draw a semi-transparent cylindrical beam from the spawn point."""
    if not show_beam:
        if DEBUG_PRINT:
            print("[DEBUG] Beam drawing skipped (show_beam=False)")
        return
    
    if DEBUG_PRINT:
        print(f"[DEBUG] Drawing beam: spawn_x={BEAM_SPAWN_X}, y_center={(BEAM_Y_MIN + BEAM_Y_MAX) / 2.0}, radius={BEAM_RADIUS}, length={BEAM_LENGTH}")
    
    glPushMatrix()
    
    # Position the beam at the spawn point, pointing along +X axis
    glTranslatef(BEAM_SPAWN_X, (BEAM_Y_MIN + BEAM_Y_MAX) / 2.0, BEAM_Z)
    
    # Rotate 90 degrees around Y axis to point cylinder along +X
    glRotatef(90, 0, 1, 0)
    
    # Disable depth test temporarily for better visibility
    glDepthMask(GL_FALSE)
    
    # Draw semi-transparent beam with a glow effect
    glEnable(GL_BLEND)
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)
    
    # Outer glow (draw first, behind) - MUCH BRIGHTER
    glColor4f(0.5, 0.8, 1.0, 0.6)
    quad = gluNewQuadric()
    gluCylinder(quad, BEAM_RADIUS, BEAM_RADIUS, BEAM_LENGTH, BEAM_SLICES, 1)
    gluDeleteQuadric(quad)
    
    # Inner bright core - VERY BRIGHT
    glColor4f(0.7, 0.9, 1.0, 0.8)
    quad2 = gluNewQuadric()
    gluCylinder(quad2, BEAM_RADIUS * 0.6, BEAM_RADIUS * 0.6, BEAM_LENGTH, BEAM_SLICES, 1)
    gluDeleteQuadric(quad2)
    
    # Draw a solid test sphere at the beam origin to verify positioning
    glColor4f(1.0, 0.0, 0.0, 1.0)
    glutSolidSphere(0.5, 10, 10)
    
    glDepthMask(GL_TRUE)
    
    glPopMatrix()

def draw_axes(size=8.0):
    glLineWidth(1.3)
    glBegin(GL_LINES)
    glColor3f(1, 0, 0);      glVertex3f(0,0,0); glVertex3f(size,0,0)
    glColor3f(0, 1, 0);      glVertex3f(0,0,0); glVertex3f(0,size,0)
    glColor3f(0, 0.6, 1);    glVertex3f(0,0,0); glVertex3f(0,0,size)
    glEnd()

def draw_trail(p):
    trail = p.trail
    n = len(trail)
    if n < 2:
        return

    glPointSize(TRAIL_POINT_SIZE)
    glBegin(GL_POINTS)
    n_m1 = max(1, n - 1)
    for i, (tx, ty, tz) in enumerate(trail):
        if i & 1:           # FIX: skip every other point (was % 3, now & 1 — faster)
            continue
        t     = i / n_m1
        alpha = 0.02 + 0.72 * (t ** 1.35)
        glColor4f(0.96, 0.96, 0.93, alpha)
        glVertex3f(tx, ty, tz)
    glEnd()

def draw_particle_heads():
    if not show_heads or not particles:
        return
    glPointSize(HEAD_POINT_SIZE)
    glBegin(GL_POINTS)
    glColor3f(1.0, 0.92, 0.25)
    for p in particles:
        glVertex3f(p.x, p.y, p.z)
    glEnd()

def draw_world_label_pointer():
    if not show_label:
        return
    glLineWidth(1.0)
    glColor4f(0.85, 0.85, 0.85, 0.42)
    glBegin(GL_LINES)
    glVertex3f(1.4, 0.8, 0.0)
    glVertex3f(7.5, 5.5, 0.0)
    glEnd()

# ---- 2D overlay helpers ----
def draw_text_2d(x, y, text, font=GLUT_BITMAP_9_BY_15):
    """Use 9x15 bitmap font — reliably rendered on Intel Mac."""
    glRasterPos2f(x, y)
    for ch in text:
        glutBitmapCharacter(font, ord(ch))

def begin_2d_overlay():
    glMatrixMode(GL_PROJECTION)
    glPushMatrix()
    glLoadIdentity()
    gluOrtho2D(0, WIDTH, 0, HEIGHT)
    glMatrixMode(GL_MODELVIEW)
    glPushMatrix()
    glLoadIdentity()
    glDisable(GL_DEPTH_TEST)

def end_2d_overlay():
    glEnable(GL_DEPTH_TEST)
    glPopMatrix()
    glMatrixMode(GL_PROJECTION)
    glPopMatrix()
    glMatrixMode(GL_MODELVIEW)

def draw_nucleus_label_overlay():
    if not show_label:
        return
    begin_2d_overlay()
    glColor3f(0.90, 0.90, 0.90)
    draw_text_2d(int(WIDTH * 0.615), int(HEIGHT * 0.595), "nucleus")
    end_2d_overlay()

def draw_hud():
    if not show_hud:
        return
    begin_2d_overlay()

    # Background panel - larger to fit more info
    glColor4f(0.04, 0.05, 0.06, 0.84)
    glBegin(GL_QUADS)
    glVertex2f(12, HEIGHT - 12);  glVertex2f(550, HEIGHT - 12)
    glVertex2f(550, HEIGHT - 250); glVertex2f(12, HEIGHT - 250)
    glEnd()

    glColor4f(0.85, 0.90, 1.0, 0.20)
    glBegin(GL_LINE_LOOP)
    glVertex2f(12, HEIGHT - 12);  glVertex2f(550, HEIGHT - 12)
    glVertex2f(550, HEIGHT - 250); glVertex2f(12, HEIGHT - 250)
    glEnd()

    glColor3f(0.95, 0.95, 0.95)
    y, line = HEIGHT - 30, 18

    lines = [
        "Rutherford Scattering  [Mac-optimized + Keyboard Camera + Beam]",
        f"Particles : {len(particles)} / {max_particles}",
        f"spawn_chance={spawn_chance:.2f}  per_tick={spawn_per_tick}",
        f"Camera: yaw={cam_yaw:.1f}  pitch={cam_pitch:.1f}  dist={cam_distance:.1f}  height={cam_height:.1f}",
        "Simulation: P pause  H hud  T heads  L label  X axes  B beam",
        "Camera:     Arrows rotate  W/S zoom  A/D yaw  Q/E height  R reset  O ortho",
        "Spawn:      [ ] chance   -/+ density   ,/. max count",
    ]
    for txt in lines:
        draw_text_2d(22, y, txt)
        y -= line

    end_2d_overlay()

# =========================================================
# Camera / projection
# =========================================================
def apply_camera():
    if ORTHO_VIEW:
        gluLookAt(0, 0, 35, 0, 0, 0, 0, 1, 0)
        return
    yaw   = math.radians(cam_yaw)
    pitch = math.radians(cam_pitch)
    cx = cam_distance * math.cos(pitch) * math.sin(yaw)
    cy = cam_distance * math.sin(pitch) + cam_height
    cz = cam_distance * math.cos(pitch) * math.cos(yaw)
    gluLookAt(cx, cy, cz, 0, cam_height, 0, 0, 1, 0)

def update_projection():
    glMatrixMode(GL_PROJECTION)
    glLoadIdentity()
    aspect = WIDTH / float(max(1, HEIGHT))
    if ORTHO_VIEW:
        vh = 22.0
        glOrtho(-vh * aspect, vh * aspect, -vh, vh, -100.0, 100.0)
    else:
        gluPerspective(45.0, aspect, 0.1, 200.0)
    glMatrixMode(GL_MODELVIEW)

def reset_camera():
    """Reset camera to default position."""
    global cam_yaw, cam_pitch, cam_distance, cam_height
    cam_yaw = DEFAULT_CAM_YAW
    cam_pitch = DEFAULT_CAM_PITCH
    cam_distance = DEFAULT_CAM_DISTANCE
    cam_height = DEFAULT_CAM_HEIGHT

# =========================================================
# GLUT callbacks
# =========================================================
def display():
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
    glLoadIdentity()
    apply_camera()

    if show_axes:
        draw_axes(8.0)

    # Draw beam cylinder first (behind particles)
    draw_beam_cylinder()

    for p in particles:
        draw_trail(p)

    draw_nucleus()
    draw_world_label_pointer()
    draw_particle_heads()
    draw_nucleus_label_overlay()
    draw_hud()

    glutSwapBuffers()

def update(_value):
    global particles, _debug_frame_counter

    if not paused:
        for _ in range(spawn_per_tick):
            if len(particles) < max_particles and random.random() < spawn_chance:
                particles.append(Particle())

        # FIX: pass DT_SUB so each substep uses the correct fractional timestep
        for _ in range(PHYSICS_SUBSTEPS):
            for p in particles:
                p.update(DT_SUB)

        # FIX: in-place slice assignment avoids creating a new list object each frame
        particles[:] = [p for p in particles if p.alive]

    _debug_frame_counter += 1
    if DEBUG_PRINT and (_debug_frame_counter % 60 == 0):
        debug_log(
            f"[dbg] n={len(particles)} paused={paused} "
            f"spawn={spawn_chance:.2f} per_tick={spawn_per_tick}"
        )

    glutPostRedisplay()
    glutTimerFunc(16, update, 0)

def reshape(w, h):
    global WIDTH, HEIGHT
    WIDTH, HEIGHT = max(1, w), max(1, h)
    glViewport(0, 0, WIDTH, HEIGHT)
    update_projection()
    glutPostRedisplay()   # FIX: force redraw after resize

def keyboard(key, x, y):
    global paused, show_hud, show_axes, show_heads, show_label, ORTHO_VIEW, show_beam
    global spawn_chance, spawn_per_tick, max_particles, particles, DEBUG_PRINT
    global cam_yaw, cam_distance, cam_height

    if isinstance(key, bytes):
        key = key.decode("utf-8", errors="ignore")

    if   key == '\x1b':       sys.exit(0)
    elif key in 'pP':         paused     = not paused
    elif key in 'hH':         show_hud   = not show_hud
    elif key in 'xX':         show_axes  = not show_axes
    elif key in 'tT':         show_heads = not show_heads
    elif key in 'lL':         show_label = not show_label
    elif key in 'bB':         
        show_beam = not show_beam
        print(f"Beam visibility: {'ON' if show_beam else 'OFF'}")
    elif key in 'oO':         
        ORTHO_VIEW = not ORTHO_VIEW
        update_projection()
    elif key in 'gG':  # Changed from 'dD' to avoid conflict with camera
        DEBUG_PRINT = not DEBUG_PRINT
        print(f"DEBUG_PRINT = {DEBUG_PRINT}", flush=True)
    
    # Camera controls (keyboard-only for Mac compatibility)
    elif key in 'wW':  cam_distance = max(5.0, cam_distance - 2.0)     # Zoom in
    elif key in 'sS':  cam_distance = min(100.0, cam_distance + 2.0)   # Zoom out
    elif key in 'aA':  cam_yaw -= 5.0                                   # Rotate left
    elif key in 'dD':  cam_yaw += 5.0                                   # Rotate right
    elif key in 'qQ':  cam_height += 1.0                                # Move up
    elif key in 'eE':  cam_height -= 1.0                                # Move down
    elif key in 'rR':  reset_camera()                                   # Reset camera
    elif key == '\r':  particles.clear()                                # Enter = clear particles
    
    # Spawn controls
    elif key == '[':  spawn_chance   = clamp(spawn_chance - 0.02, 0.0, 1.0)
    elif key == ']':  spawn_chance   = clamp(spawn_chance + 0.02, 0.0, 1.0)
    elif key == '-':  spawn_per_tick = max(1,    spawn_per_tick - 1)
    elif key == '+':  spawn_per_tick = min(10,   spawn_per_tick + 1)
    elif key == ',':
        max_particles = max(50, max_particles - 50)
        if len(particles) > max_particles:
            particles[:] = particles[:max_particles]
    elif key == '.':  max_particles  = min(3000, max_particles + 50)

    glutPostRedisplay()

def special_keys(key, x, y):
    global cam_yaw, cam_pitch
    if   key == GLUT_KEY_LEFT:  cam_yaw   -= 5.0                        # Rotate left
    elif key == GLUT_KEY_RIGHT: cam_yaw   += 5.0                        # Rotate right
    elif key == GLUT_KEY_UP:    cam_pitch  = clamp(cam_pitch + 3.0, -85.0, 85.0)  # Look up
    elif key == GLUT_KEY_DOWN:  cam_pitch  = clamp(cam_pitch - 3.0, -85.0, 85.0)  # Look down
    glutPostRedisplay()

# =========================================================
# Init
# =========================================================
def init():
    global _nucleus_dl

    glEnable(GL_DEPTH_TEST)
    glShadeModel(GL_SMOOTH)
    glClearColor(0.05, 0.05, 0.06, 1.0)

    glEnable(GL_BLEND)
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)

    try:
        glEnable(GL_POINT_SMOOTH)
        glHint(GL_POINT_SMOOTH_HINT, GL_NICEST)
        glHint(GL_LINE_SMOOTH_HINT,  GL_NICEST)
    except Exception:
        pass  # not fatal on all drivers

    update_projection()

    # FIX: compile nucleus sphere into a display list — drawn from GPU cache each frame
    _nucleus_dl = build_nucleus_display_list()
    
    # Debug: print beam settings
    print(f"\n[DEBUG] Beam settings:")
    print(f"  BEAM_SPAWN_X = {BEAM_SPAWN_X}")
    print(f"  BEAM_Y_MIN = {BEAM_Y_MIN}, BEAM_Y_MAX = {BEAM_Y_MAX}")
    print(f"  BEAM_Z = {BEAM_Z}")
    print(f"  BEAM_RADIUS = {BEAM_RADIUS}")
    print(f"  BEAM_LENGTH = {BEAM_LENGTH}")
    print(f"  show_beam = {show_beam}")
    print(f"  Press 'B' to toggle beam visibility\n")

# =========================================================
# Main
# =========================================================
def main():
    # FIX: pass empty list to glutInit instead of sys.argv
    # sys.argv can cause a GLUT argument-parse crash on macOS
    glutInit([])
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH)
    glutInitWindowSize(WIDTH, HEIGHT)
    glutCreateWindow(b"Rutherford Alpha Scattering - Keyboard Camera (Mac)")

    init()

    glutDisplayFunc(display)
    glutReshapeFunc(reshape)
    glutKeyboardFunc(keyboard)
    glutSpecialFunc(special_keys)
    glutTimerFunc(0, update, 0)

    print("\n" + "="*60)
    print("Rutherford Scattering - Mac-Optimized Keyboard Controls")
    print("="*60)
    print("CAMERA CONTROLS:")
    print("  Arrow Keys - Rotate camera (yaw and pitch)")
    print("  W/S        - Zoom in/out")
    print("  A/D        - Rotate left/right")
    print("  Q/E        - Move camera up/down")
    print("  R          - Reset camera to default")
    print("  O          - Toggle orthographic/perspective view")
    print("\nSIMULATION:")
    print("  P          - Pause/unpause")
    print("  B          - Toggle beam cylinder visibility")
    print("  Enter      - Clear all particles")
    print("  H          - Toggle HUD")
    print("  ESC        - Exit")
    print("="*60 + "\n")

    glutMainLoop()

if __name__ == "__main__":
    main()

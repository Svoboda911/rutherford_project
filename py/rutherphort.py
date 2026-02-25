import sys
import random
import math
from collections import deque

from OpenGL.GL import *
from OpenGL.GLUT import *
from OpenGL.GLU import *
from OpenGL.GLUT import GLUT_BITMAP_9_BY_15, GLUT_BITMAP_HELVETICA_12

# =========================================================
# Window
# =========================================================
WIDTH, HEIGHT = 1100, 700

# =========================================================
# Physics / Visualization (stylized Rutherford beam)
# =========================================================
# NOTE: This is a visual simulation, not SI-unit exact physics.
K_COULOMB = 2.5
NUCLEUS_CHARGE = 79.0
ALPHA_CHARGE = 2.0

# Smaller dt + substeps = smoother near nucleus
DT = 0.0018
PHYSICS_SUBSTEPS = 2

# Softening + cap to avoid numerical blow-up while allowing close passes
FORCE_SOFTENING = 0.14       # larger -> gentler near-center force
MIN_R_DIR = 0.08             # only for direction normalization
MAX_ACCEL = 2600.0           # numerical safety clamp

# =========================================================
# Beam / View mode
# =========================================================
ORTHO_VIEW = True            # flat technical look
BEAM_SPAWN_X = -44.0
BEAM_Y_MIN = -5.0            # narrower beam => more near-nucleus passes
BEAM_Y_MAX = 5.0
BEAM_SPEED = 28.0
BEAM_Z = 0.0                 # force true 2D look

# =========================================================
# Particle system / Rendering
# =========================================================
particles = []
spawn_chance = 0.10
spawn_per_tick = 2
max_particles = 180
particle_lifetime = 18.0
trail_length = 500

# Visual sizes
NUCLEUS_DRAW_RADIUS = 0.40   # visual only (separate from physics behavior)
HEAD_POINT_SIZE = 2.6
TRAIL_POINT_SIZE = 1.3

# Toggles
paused = False
show_hud = True
show_axes = False
show_heads = True
show_label = True

# Debug
DEBUG_PRINT = False
_debug_frame_counter = 0

# =========================================================
# Camera (fallback if ORTHO_VIEW=False)
# =========================================================
cam_yaw = 0.0
cam_pitch = 15.0
cam_distance = 35.0

# =========================================================
# Utilities
# =========================================================
def clamp(v, lo, hi):
    return max(lo, min(hi, v))

def debug_log(msg):
    if DEBUG_PRINT:
        print(msg)

# =========================================================
# Particle
# =========================================================
class Particle:
    __slots__ = (
        "x", "y", "z",
        "vx", "vy", "vz",
        "age", "alive", "trail"
    )

    def __init__(self):
        # Parallel-ish beam entering from the left
        self.x = BEAM_SPAWN_X
        self.y = random.uniform(BEAM_Y_MIN, BEAM_Y_MAX)
        self.z = BEAM_Z

        self.vx = BEAM_SPEED
        self.vy = random.uniform(-0.01, 0.01)
        self.vz = 0.0

        self.age = 0.0
        self.alive = True

        self.trail = deque(maxlen=trail_length)
        self.trail.append((self.x, self.y, self.z))

    def update(self):
        if not self.alive:
            return

        # Position relative to nucleus at origin
        rx, ry, rz = self.x, self.y, self.z
        r2 = rx * rx + ry * ry + rz * rz
        if r2 < 1e-12:
            r2 = 1e-12
        r = math.sqrt(r2)

        # Direction normalization radius (avoid division by 0)
        r_dir = max(r, MIN_R_DIR)

        # Softened repulsive Coulomb force (stylized)
        denom = r2 + FORCE_SOFTENING * FORCE_SOFTENING
        accel_mag = (K_COULOMB * NUCLEUS_CHARGE * ALPHA_CHARGE) / denom

        # Numerical safety cap (prevents ugly spikes / teleporting)
        if accel_mag > MAX_ACCEL:
            accel_mag = MAX_ACCEL

        scale = accel_mag / r_dir
        ax = rx * scale
        ay = ry * scale
        az = rz * scale

        # Semi-implicit Euler
        self.vx += ax * DT
        self.vy += ay * DT
        self.vz += az * DT

        self.x += self.vx * DT
        self.y += self.vy * DT
        self.z += self.vz * DT

        self.age += DT
        self.trail.append((self.x, self.y, self.z))

        # Cleanup bounds
        if (
            self.x > 92.0 or self.x < -65.0 or
            abs(self.y) > 46.0 or
            abs(self.z) > 8.0 or
            self.age > particle_lifetime
        ):
            self.alive = False

# =========================================================
# Drawing helpers
# =========================================================
def draw_sphere(x, y, z, radius, color, slices=16, stacks=16):
    glPushMatrix()
    glTranslatef(x, y, z)
    glColor3f(*color)
    glutSolidSphere(radius, slices, stacks)
    glPopMatrix()

def draw_axes(size=8.0):
    glLineWidth(1.3)
    glBegin(GL_LINES)
    # X
    glColor3f(1, 0, 0)
    glVertex3f(0, 0, 0)
    glVertex3f(size, 0, 0)
    # Y
    glColor3f(0, 1, 0)
    glVertex3f(0, 0, 0)
    glVertex3f(0, size, 0)
    # Z
    glColor3f(0, 0.6, 1)
    glVertex3f(0, 0, 0)
    glVertex3f(0, 0, size)
    glEnd()

def draw_nucleus():
    draw_sphere(
        0.0, 0.0, 0.0,
        NUCLEUS_DRAW_RADIUS,
        (1.0, 0.84, 0.12),
        slices=20, stacks=20
    )

def draw_trail(p):
    """Clean dotted trajectory."""
    if len(p.trail) < 2:
        return

    glPointSize(TRAIL_POINT_SIZE)
    glBegin(GL_POINTS)

    n = len(p.trail)
    for i, (tx, ty, tz) in enumerate(p.trail):
        # Skip points for dotted look + less fog
        if i % 3 != 0:
            continue

        t = i / max(1, n - 1)      # 0 oldest -> 1 newest
        alpha = 0.02 + 0.72 * (t ** 1.35)

        # warm-white dots
        glColor4f(0.96, 0.96, 0.93, alpha)
        glVertex3f(tx, ty, tz)

    glEnd()

def draw_particle_heads():
    """Draw all particle heads as points (faster + cleaner than spheres)."""
    if not show_heads or not particles:
        return

    glPointSize(HEAD_POINT_SIZE)
    glBegin(GL_POINTS)
    for p in particles:
        # slightly warm bright head
        glColor3f(1.0, 0.92, 0.25)
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

# ------------------------- 2D overlay helpers -------------------------
def draw_text_2d(x, y, text, font=None):
    # Avoid VS Code/Pylance warning on default GLUT constant in signature
    if font is None:
        font = GLUT_BITMAP_9_BY_15

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
    draw_text_2d(int(WIDTH * 0.615), int(HEIGHT * 0.595), "nucleus", GLUT_BITMAP_HELVETICA_12)
    end_2d_overlay()

def draw_hud():
    if not show_hud:
        return

    begin_2d_overlay()

    glColor4f(0.04, 0.05, 0.06, 0.84)
    glBegin(GL_QUADS)
    glVertex2f(12, HEIGHT - 12)
    glVertex2f(510, HEIGHT - 12)
    glVertex2f(510, HEIGHT - 188)
    glVertex2f(12, HEIGHT - 188)
    glEnd()

    glColor4f(0.85, 0.90, 1.0, 0.20)
    glBegin(GL_LINE_LOOP)
    glVertex2f(12, HEIGHT - 12)
    glVertex2f(510, HEIGHT - 12)
    glVertex2f(510, HEIGHT - 188)
    glVertex2f(12, HEIGHT - 188)
    glEnd()

    glColor3f(0.95, 0.95, 0.95)
    y = HEIGHT - 32
    line = 18

    draw_text_2d(22, y, "Rutherford Scattering (clean beam mode)"); y -= line
    draw_text_2d(22, y, f"Particles: {len(particles)} / {max_particles}"); y -= line
    draw_text_2d(22, y, f"spawn chance={spawn_chance:.2f}, per tick={spawn_per_tick}"); y -= line
    draw_text_2d(22, y, f"k={K_COULOMB:.1f}, dt={DT:.4f}, substeps={PHYSICS_SUBSTEPS}"); y -= line
    draw_text_2d(22, y, f"soft={FORCE_SOFTENING:.2f}, beam Y=[{BEAM_Y_MIN:.1f},{BEAM_Y_MAX:.1f}]"); y -= line
    draw_text_2d(22, y, "Keys: P pause  R reset  H HUD  T heads  L label  X axes"); y -= line
    draw_text_2d(22, y, "      [ ] spawn chance   -/+ density   ,/. max count   D debug")

    end_2d_overlay()

# =========================================================
# Camera / projection
# =========================================================
def apply_camera():
    if ORTHO_VIEW:
        gluLookAt(0.0, 0.0, 35.0,
                  0.0, 0.0, 0.0,
                  0.0, 1.0, 0.0)
        return

    yaw = math.radians(cam_yaw)
    pitch = math.radians(cam_pitch)

    cx = cam_distance * math.cos(pitch) * math.sin(yaw)
    cy = cam_distance * math.sin(pitch)
    cz = cam_distance * math.cos(pitch) * math.cos(yaw)

    gluLookAt(cx, cy, cz, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0)

def update_projection():
    glMatrixMode(GL_PROJECTION)
    glLoadIdentity()

    aspect = WIDTH / float(max(1, HEIGHT))

    if ORTHO_VIEW:
        view_h = 22.0
        view_w = view_h * aspect
        glOrtho(-view_w, view_w, -view_h, view_h, -100.0, 100.0)
    else:
        gluPerspective(45.0, aspect, 0.1, 200.0)

    glMatrixMode(GL_MODELVIEW)

# =========================================================
# GLUT callbacks
# =========================================================
def display():
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
    glLoadIdentity()

    apply_camera()

    if show_axes:
        draw_axes(8.0)

    # Trails first
    for p in particles:
        draw_trail(p)

    # Nucleus + pointer
    draw_nucleus()
    draw_world_label_pointer()

    # Heads last (on top)
    draw_particle_heads()

    # Overlays
    draw_nucleus_label_overlay()
    draw_hud()

    glutSwapBuffers()

def update(_value):
    global particles, _debug_frame_counter

    if not paused:
        # Spawn new particles (beam density)
        for _ in range(spawn_per_tick):
            if len(particles) < max_particles and random.random() < spawn_chance:
                particles.append(Particle())

        # Physics substeps (improves near-center smoothness)
        for _ in range(PHYSICS_SUBSTEPS):
            for p in particles:
                p.update()

        # Remove dead particles
        particles = [p for p in particles if p.alive]

    # Throttled debug output
    _debug_frame_counter += 1
    if DEBUG_PRINT and (_debug_frame_counter % 60 == 0):
        debug_log(
            f"[debug] particles={len(particles)} "
            f"paused={paused} spawn={spawn_chance:.2f} per_tick={spawn_per_tick}"
        )

    glutPostRedisplay()
    glutTimerFunc(16, update, 0)  # ~60 FPS

def reshape(w, h):
    global WIDTH, HEIGHT
    WIDTH = max(1, w)
    HEIGHT = max(1, h)

    glViewport(0, 0, WIDTH, HEIGHT)
    update_projection()

def keyboard(key, x, y):
    global paused, show_hud, show_axes, show_heads, show_label
    global spawn_chance, spawn_per_tick, max_particles, particles, DEBUG_PRINT

    if isinstance(key, bytes):
        key = key.decode("utf-8", errors="ignore")

    if key == '\x1b':  # ESC
        sys.exit(0)

    # Toggles
    elif key in ('p', 'P'):
        paused = not paused
    elif key in ('r', 'R'):
        particles.clear()
    elif key in ('h', 'H'):
        show_hud = not show_hud
    elif key in ('x', 'X'):
        show_axes = not show_axes
    elif key in ('t', 'T'):
        show_heads = not show_heads
    elif key in ('l', 'L'):
        show_label = not show_label
    elif key in ('d', 'D'):
        DEBUG_PRINT = not DEBUG_PRINT
        print(f"DEBUG_PRINT = {DEBUG_PRINT}")

    # Spawn chance
    elif key == '[':
        spawn_chance = clamp(spawn_chance - 0.02, 0.0, 1.0)
    elif key == ']':
        spawn_chance = clamp(spawn_chance + 0.02, 0.0, 1.0)

    # Beam density (particles spawned per tick)
    elif key == '-':
        spawn_per_tick = max(1, spawn_per_tick - 1)
    elif key == '+':
        spawn_per_tick = min(10, spawn_per_tick + 1)

    # Max particles
    elif key == ',':
        max_particles = max(50, max_particles - 50)
        if len(particles) > max_particles:
            particles[:] = particles[:max_particles]
    elif key == '.':
        max_particles = min(3000, max_particles + 50)

    glutPostRedisplay()

def special_keys(key, x, y):
    global cam_yaw, cam_pitch

    if key == GLUT_KEY_LEFT:
        cam_yaw -= 3.0
    elif key == GLUT_KEY_RIGHT:
        cam_yaw += 3.0
    elif key == GLUT_KEY_UP:
        cam_pitch = clamp(cam_pitch + 2.0, -85.0, 85.0)
    elif key == GLUT_KEY_DOWN:
        cam_pitch = clamp(cam_pitch - 2.0, -85.0, 85.0)

    glutPostRedisplay()

# =========================================================
# Init
# =========================================================
def init():
    glEnable(GL_DEPTH_TEST)
    glShadeModel(GL_SMOOTH)

    # Dark background
    glClearColor(0.05, 0.05, 0.06, 1.0)

    # Blending for trails / HUD
    glEnable(GL_BLEND)
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)

    # Smoothing (best effort; may vary by platform)
    try:
        glEnable(GL_POINT_SMOOTH)
        glHint(GL_POINT_SMOOTH_HINT, GL_NICEST)
        glHint(GL_LINE_SMOOTH_HINT, GL_NICEST)
    except Exception:
        pass

    update_projection()

# =========================================================
# Main
# =========================================================
def main():
    glutInit(sys.argv)
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH)
    glutInitWindowSize(WIDTH, HEIGHT)
    glutCreateWindow(b"Rutherford Alpha Scattering - Clean / Debugged")

    init()

    glutDisplayFunc(display)
    glutReshapeFunc(reshape)
    glutKeyboardFunc(keyboard)
    glutSpecialFunc(special_keys)
    glutTimerFunc(0, update, 0)

    glutMainLoop()

if __name__ == "__main__":
    main()

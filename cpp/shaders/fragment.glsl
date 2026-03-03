#version 330 core
in vec4 ourColor;
in vec3 FragPos;
in vec3 Normal;
out vec4 FragColor;

uniform bool useLighting;
uniform vec3 lightPos;
uniform vec3 viewPos;

void main() {
    if (!useLighting) {
        FragColor = ourColor;
        return;
    }

    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);

    float ambientStrength = 0.28;
    float diffuse = max(dot(norm, lightDir), 0.0);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 20.0);

    vec3 ambient = ambientStrength * ourColor.rgb;
    vec3 diffuseColor = 0.72 * diffuse * ourColor.rgb;
    vec3 specular = 0.33 * spec * vec3(1.0);

    FragColor = vec4(ambient + diffuseColor + specular, ourColor.a);
}

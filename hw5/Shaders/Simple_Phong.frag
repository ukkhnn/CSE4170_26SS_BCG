#version 330

uniform struct LightInfo {
  vec4 Position;       // Light position in eye coords
  vec3 La;
  vec3 L;              // Direct light intensity
  vec3 Spot_direction; // Spot direction in eye coords
  float Spot_cutoff_angle; // in degrees in [0, 90]
  float Spot_exponent;
} Light;

uniform struct MaterialInfo {
  vec3 Ka;            // Ambient reflectivity
  vec3 Kd;            // Diffuse reflectivity
  vec3 Ks;            // Specular reflectivity
  float Shininess;    // Specular shininess factor
} Material;

uniform bool use_halfway_vector;
uniform bool apply_spot_light;

uniform bool  apply_attenuation;
uniform vec3  attenuation_coeff;
uniform bool  u_use_texture;
uniform sampler2D u_base_texture;
uniform vec2  u_tex_scale;
uniform float u_alpha;
uniform bool  apply_toon;
uniform bool  apply_fog;
uniform vec3  fog_color;

in vec3 Position_EC;
in vec3 Normal_EC;
in vec2 TexCoord;

layout(location = 0) out vec4 FragColor;

const float TOON_LEVELS = 4.0;
const float FOG_START   = 1000.0;
const float FOG_END     = 4000.0;

vec3 PhongShading(vec3 position, vec3 n, vec3 base_Ka, vec3 base_Kd) {
    vec3 v = normalize(-position);
    vec3 color_sum = Light.La * base_Ka;
    vec3 s = normalize(Light.Position.xyz - position);

    float local_scale_factor = 1.0;
    if (apply_spot_light) {
        float Spot_cutoff_angle = clamp(Light.Spot_cutoff_angle, 0.0, 90.0);
        vec3 spot_dir = normalize(Light.Spot_direction);

        float tmp_float = dot(-s, spot_dir);
        if (tmp_float >= cos(radians(Spot_cutoff_angle))) {
            local_scale_factor = pow(tmp_float, Light.Spot_exponent);
        }
        else {
            local_scale_factor = 0.0;
        }
    }

    float attenuation = 1.0;
    if (apply_attenuation) {
        float d = length(Light.Position.xyz - position);
        attenuation = 1.0 / (attenuation_coeff.x
                           + attenuation_coeff.y * d
                           + attenuation_coeff.z * d * d);
    }

    if (local_scale_factor > 0.0) {
        vec3 diffuse = vec3(0.0);
        vec3 spec = vec3(0.0);

        float sDotN = max(dot(s, n), 0.0);
        if (sDotN > 0.0) {
            float spec_f;
            if (use_halfway_vector) {
                vec3 h = normalize(s + v);
                spec_f = pow(max(dot(n, h), 0.0), Material.Shininess);
            }
            else {
                vec3 r = reflect(-s, n);
                spec_f = pow(max(dot(r, v), 0.0), Material.Shininess);
            }

            if (apply_toon) {
                float q = ceil(sDotN * TOON_LEVELS) / TOON_LEVELS;
                diffuse = base_Kd * q;
                spec = Material.Ks * step(0.5, spec_f);
            }
            else {
                diffuse = base_Kd * sDotN;
                spec = Material.Ks * spec_f;
            }
        }
        color_sum += Light.L * (diffuse + spec) * local_scale_factor * attenuation;
    }

    if (apply_toon) {
        float edge = smoothstep(0.0, 0.3, dot(n, v));
        color_sum *= edge;
    }
    return color_sum;
}

void main() {
    vec3 tex_color = u_use_texture ? texture(u_base_texture, TexCoord * u_tex_scale).rgb
                                   : vec3(1.0);
    vec3 base_Kd = u_use_texture ? tex_color : Material.Kd;
    vec3 base_Ka = u_use_texture ? tex_color : Material.Ka;

    vec3 color = PhongShading(Position_EC, normalize(Normal_EC), base_Ka, base_Kd);

    if (apply_fog) {
        float dist = length(Position_EC);
        float f = clamp((FOG_END - dist) / (FOG_END - FOG_START), 0.0, 1.0);
        color = mix(fog_color, color, f);
    }

    FragColor = vec4(color, u_alpha);
}

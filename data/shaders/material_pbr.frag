#version 450

layout (location = 0) in vec3 in_color;
layout (location = 1) in vec3 in_position;
layout (location = 2) in vec3 in_normal;
layout (location = 3) in vec2 in_uv;
layout (location = 4) in vec3 in_tangent;

layout (location = 0) out vec4 out_color;

struct light
{
    vec4 position; // w : 0.0f for directional light, 1.0f for point light
    vec4 color;    // w : intensity
};

layout (set = 0, binding = 0) uniform global_ubo
{
    mat4 projection;
    mat4 view;
    mat4 inverse_view;
    vec4 ambient_light_color; // w is intensity
    light lights[10];
    int num_lights;
} ubo;

#define PI 3.1415926535897932384626433832795

const vec3 dielectric = vec3(0.04f);
const float ambient = 0.01f;

layout (push_constant) uniform Push
{
    mat4  model_matrix;
    mat4  normal_matrix;
    float r;
    float g;
    float b;
    float metallic;
    float roughness;
} push;

/*
    * light          : light source
    * point_to_shade : target position
    * return         : light intensity - toal amount of energy the light emits (~radiant intensity)
*/
vec3 radiance(light light, vec3 point_to_shade)
{
    if (light.position.w == 0.0f)
    {
        return light.color.rgb * light.color.w;
    }
    vec3 dir_to_light = light.position.xyz - point_to_shade;
    float attenuation = 1.0f / dot(dir_to_light, dir_to_light);
    return light.color.rgb * light.color.w * attenuation;
}

/*
    * cd : diffuse color (color of the material)
    * kd : diffuse reflectio coefficient: how reflective is this matee material, should range [0, 1]
*/
vec3 lambert(vec3 cd, vec3 kd)
{
/*
        * rho: reflectivity of the surface, or to be more accurate, the perfect diffuse reflectance
    */
    vec3 rho = cd * kd;
    return rho / PI;
}

/*
    * n_dot_h : dot product between normal and half vector
    * a       : roughness value squared
*/
float trowbridge_reitz_ggx(float n_dot_h, float a)
{
    float a2  = a * a;
    float num = a2;

    float n_dot_h2 = n_dot_h * n_dot_h;
    float denom    = n_dot_h2 * (a2 - 1.0f) + 1.0f;

    return num / (PI * denom * denom);
}

/*
    * h_dot_v : dot product between half vector and view direction
    * f0      : base reflectivity of a surface on IOR (Indices Of Refrection), this is different for dielectrics (non-metals) and conductors (metals)
*/
vec3 fresnel_schlick(float h_dot_v, vec3 f0)
{
    return f0 + (1.0f - f0) * pow(1.0f - h_dot_v, 5.0f);
}

/*
    * n_dot_v : dot product between normal and view direction
    * n_dot_l : dot product between normal and light direction
    * a       : roughness value squared
*/
float geometry_schlick_smith_ggx(float n_dot_v, float n_dot_l, float a)
{
/*
        * k_direct:a (roughness squared) remapped based on whether you use the function with direction or indirect lighting
    */
    float num = a + 1.0f;
    float num2 = num * num;
    float k_direct = num2 / 8.0f;

    float g1 = n_dot_v / (n_dot_v * (1.0f - k_direct) + k_direct);
    float g2 = n_dot_l / (n_dot_l * (1.0f - k_direct) + k_direct);
    return g1 * g2;
}

/*
    * n         : surface normal
    * l         : normalized light direction
    * v         : normalized view direction
    * albedo    : color of the material
    * metallic  : 0.0f for dielectrics (non-metals) and 1.0f for conductors (metals)
    * roughness : roughness of the material
    * return    : specular reflectance
*/
vec3 brdf(vec3 n, vec3 l, vec3 v, vec3 albedo, float metallic, float roughness)
{
/*
        * f0: base reflectivity of a surface on IOR (Indices Of Refrection), this is different for dielectrics (non-metals) and conductors (metals)
        * h : half vector between light direction and view direction
        * a : roughness value squared
    */
    vec3 f0 = metallic == 0.0f ? dielectric : albedo;
    vec3 h  = normalize(l + v);
    float a = roughness * roughness;

    float n_dot_h = clamp(dot(n, h), 0.0f, 1.0f);
    float n_dot_l = clamp(dot(n, l), 0.0f, 1.0f);
    float n_dot_v = clamp(dot(n, v), 0.0f, 1.0f);
    float h_dot_v = clamp(dot(h, v), 0.0f, 1.0f);


/*
        * d     : normal distribution function: how many microfacets point in right direction?
        * f     : fresnel function: how reflective are the microfacets?
        * g     : geometry function: how much shadowing and masking is happening because of the microfacets?
        * denom : reprojection of the factors
    */
    float d = trowbridge_reitz_ggx(n_dot_h, a);
    vec3 f  = fresnel_schlick(h_dot_v, f0);
    float g = geometry_schlick_smith_ggx(n_dot_v, n_dot_l, a);
    float denom = 4.0f * n_dot_v * n_dot_l;

    return d * f * g / denom;
}

void main()
{
    vec3 camera_pos_world = ubo.inverse_view[3].xyz;
    vec3 view_dir         = normalize(camera_pos_world - in_position);

    vec3 base_color = vec3(push.r, push.g, push.b);
    float metallic  = push.metallic;
    float roughness = push.roughness;

    out_color.rgb = base_color * ambient;
    out_color.a   = 1.0f;
    
    light lights[4];
    lights[0].color = vec4(1.0f, 1.0f, 1.0f, 1.0f);
    lights[0].position = vec4(10.0f, -5.0f, -10.0f, 0.0f);
    
    lights[1].color = vec4(1.0f, 1.0f, 1.0f, 1.0f);
    lights[1].position = vec4(10.0f, -5.0f, 10.0f, 0.0f);
    
    lights[2].color = vec4(1.0f, 1.0f, 1.0f, 1.0f);
    lights[2].position = vec4(-10.0f, -5.0f, -10.0f, 0.0f);
    
    lights[3].color = vec4(1.0f, 1.0f, 1.0f, 1.0f);
    lights[3].position = vec4(-10.0f, -5.0f, 10.0f, 0.0f);

    for (int i = 0; i < 4; ++i)
    {
        light light         = lights[i];
        vec3 l              = normalize(light.position.xyz - in_position);
        float observed_area = clamp(dot(in_normal, l), 0.0f, 1.0f);

        if (observed_area > 0.0f)
        {
            vec3 specular = brdf(in_normal, l, view_dir, base_color, metallic, roughness);

            vec3 kd      = metallic == 0.0f ? 1.0f - specular : vec3(0.0f);
            vec3 diffuse = lambert(base_color, kd);

            out_color.rgb +=  radiance(light, in_position) * (diffuse + specular) * observed_area;
        }
    }
}

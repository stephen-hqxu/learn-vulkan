#version 460 core
#extension GL_EXT_ray_query : require
#include "SimpleWater.glsl"
#include "CameraData.glsl"
#define PLANE_HIDE_PLANE_PROPERTY
#define PLANE_VERTEX_ACCESS readonly
#define PLANE_INDEX_ACCESS readonly
#include "PlaneGeometry.glsl"

layout(early_fragment_tests) in;

WATER_RAY_PROPERTY(in);

layout(location = 0) out vec4 FragColour;

//This scene should consist of exactly one plane geometry.
layout(set = 1, binding = 1) uniform accelerationStructureEXT Scene;
layout(set = 1, binding = 2) uniform sampler2D WaterTextureCollection[4];

#define INDEX_WATER_TEXTURE(IDX) WaterTextureCollection[IDX]
#define SceneTexture INDEX_WATER_TEXTURE(0u)
#define WaterNormal INDEX_WATER_TEXTURE(1u)
#define WaterDistortion INDEX_WATER_TEXTURE(2u)
#define SceneDepth INDEX_WATER_TEXTURE(3u)

layout(std430, push_constant) readonly restrict uniform Argument {
	PlaneVertex Vertex;
	PlaneIndex Index;
	float AnimationTimer;//increment and wrapped over between [0.0f, NormalScale)
};

//HACK: We treat water as a perfect flat plane that points directly upwards.
//If we wish to incorporate complex vertex animation (like water waves), then we should avoid using hard-coded value.
//This matrix is to convert from tangent to world space for upward-facing plane.
const mat3 PlaneTBN = mat3(
	vec3(1.0f, 0.0f, 0.0f),
	vec3(0.0f, 0.0f, -1.0f),
	vec3(0.0f, 1.0f, 0.0f)
);

const float minRayTime = 1e-4f, maxRayTime = 1e4f;

//Return hit colour and ray length.
vec4 findHitColour(const vec3 dir) {
	rayQueryEXT query;
	rayQueryInitializeEXT(query, Scene, gl_RayFlagsOpaqueEXT, 0xFFu, RayOrigin,
		minRayTime, dir, maxRayTime);

	//where the magic happens...
	rayQueryProceedEXT(query);
	if (rayQueryGetIntersectionTypeEXT(query, true) != gl_RayQueryCommittedIntersectionTriangleEXT) {
		//miss
		return vec4(Water.SkyColour, maxRayTime);
	}
	//closest hit
	const vec2 bary2 = rayQueryGetIntersectionBarycentricsEXT(query, true);
	const vec3 bary3 = vec3(1.0f - bary2.x - bary2.y, bary2);
	
	const uint primitive_idx = rayQueryGetIntersectionPrimitiveIndexEXT(query, true);
	//the index in our geometry is packed as 6 integers per structure,
	//while a primitive is packed as 3 integers per structure; need to convert the index.
	const uint structure_idx = primitive_idx >> 1u,//equivalent to `primitive_idx / 2u`
		within_structure_subset = primitive_idx & 1u;//equivalent to `primitive_idx % 2u`

	vec2 hit_uv = vec2(0.0f);
	for (uint i = 0u; i < bary3.length(); i++) {
		const uint current_index = Index[structure_idx].Index[3u * within_structure_subset + i];
		const vec2 vertex_uv = vec2(Vertex[current_index].UV) / float(~0us);

		hit_uv += bary3[i] * vertex_uv;
	}
	const vec3 out_colour = textureLod(SceneTexture, hit_uv, 0.0f).rgb;
	return vec4(pow(out_colour, vec3(2.2f)), rayQueryGetIntersectionTEXT(query, true));
}

void main() {
	/*
	So basically we use rasterisation to render the primary visibility (a.k.a., primary ray),
	because rasterisation is extremely efficient of doing so.
	Then we use ray query to find the closest hit of reflection and refraction rays.
	The closest hit colour will be our reflection and refraction colour.
	*/
	//calculate water animation
	const vec2 scaled_uv = TexCoord * Water.NormalScale,
		uv_distortion1 = texture(WaterDistortion, scaled_uv + vec2(AnimationTimer, 0.0f)).rg,
		uv_distortion2 = texture(WaterDistortion, scaled_uv + vec2(0.0f, AnimationTimer)).rg,
		distortion = ((uv_distortion1 + uv_distortion2) * 2.0f - 1.0f) * Water.DistortionStrength;

	const vec3 normal_tangent = normalize(texture(WaterNormal, distortion).rgb) * 2.0f - 1.0f,
		normal_world = PlaneTBN * normal_tangent,//in world space

		water_normal = mix(vec3(0.0f, 1.0f, 0.0f), normal_world, Water.NormalStrength),
		primary_ray_dir = normalize(RayOrigin - Camera.Position),//pointing from camera water surface

		reflection_dir = reflect(primary_ray_dir, water_normal),
		refraction_dir = refract(primary_ray_dir, water_normal, Water.IoR);
	const float frenel_factor = pow(
		clamp(dot(-primary_ray_dir, water_normal), 0.0f, 1.0f),
		Water.FresnelScale
	);

	/*
	Trace and find intersection of reflection/refraction ray.
	*/
	const vec4 reflection_intersection = findHitColour(normalize(reflection_dir)),
		refraction_intersection = findHitColour(normalize(refraction_dir));

	/*
	Due to LoD difference of geometry presented in rendered scene and acceleration structure,
	there is a chance of *self intersection*.
	We try to make the water translucent around the points where water intersect with the scene.
	*/
	const vec2 ndc = gl_FragCoord.xy / textureSize(SceneDepth, 0).xy;
	const float scene_distance = lineariseDepth(textureLod(SceneDepth, ndc, 0.0f).r),
		water_distance = lineariseDepth(gl_FragCoord.z),
		water_depth = abs(scene_distance - water_distance),
		transparency = smoothstep(0.0f, Water.TransparencyDepth, water_depth),
		tint_factor = smoothstep(0.0f, Water.DepthOfInvisibility, refraction_intersection.a);
	
	const vec3 reflection_colour = reflection_intersection.rgb,
		refraction_colour = mix(refraction_intersection.rgb, Water.WaterTint, tint_factor),
		water_colour = mix(reflection_colour, refraction_colour, frenel_factor);

	//A transparency of 1.0 means we should render purely using water colour;
	//0.0 means the water colour should not be visible at all.
	FragColour = vec4(water_colour, transparency);
}
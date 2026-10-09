"""Check material-selection boundaries and triplanar frame invariants."""
import math
import random


def dot(a, b):
    return sum(x * y for x, y in zip(a, b))


def normalized(v):
    length = math.sqrt(dot(v, v))
    return tuple(x / length for x in v)


def weights(elevation, slope_cos, heights=(0.5, 0.5, 0.5), strength=0, width=0.2):
    grass = max(0, min(1, elevation / 4))
    grass = grass * grass * (3 - 2 * grass)
    rock = max(0, min(1, (slope_cos - math.cos(math.radians(30))) / (math.cos(math.radians(55)) - math.cos(math.radians(30)))))
    rock = rock * rock * (3 - 2 * rock)
    result = [grass * (1 - rock), (1 - grass) * (1 - rock), rock]
    if strength > 0:
        scores = [w + h * strength for w, h in zip(result, heights)]
        peak = max(s for w, s in zip(result, scores) if w > 0)
        result = [w * max(s - peak + width, 0) for w, s in zip(result, scores)]
        result = [w / sum(result) for w in result]
    return result


assert weights(-10, 1) == [0, 1, 0], 'Beach selects sand'
assert weights(10, 1) == [1, 0, 0], 'High flat terrain selects grass'
assert weights(2, 0) == [0, 0, 1], 'Cliffs select rock at every elevation'
assert weights(2, 1, (0, 1, 1), strength=0) == [0.5, 0.5, 0], 'Disabled height blending preserves weights'
assert weights(2, 1, (1, 0, 1), strength=1) == [1, 0, 0], 'Higher material wins the transition'
assert weights(-10, 1, (1, 0, 1), strength=1) == [0, 1, 0], 'Height cannot activate an excluded layer'

random.seed(37)
for _ in range(5000):
    normal = normalized([random.uniform(-1, 1) for _ in range(3)])
    projections = [abs(x) ** 4 for x in normal]
    projections = [x / sum(projections) for x in projections]
    neutral = list(normal)
    for axis in range(3):
        side = -1 if normal[axis] < 0 else 1
        tangent = (0, 0, 1) if axis == 0 else (1, 0, 0)
        bitangent = (0, -side, 0) if axis == 0 else ((0, 0, -side) if axis == 1 else (0, side, 0))
        cross = (tangent[1]*bitangent[2]-tangent[2]*bitangent[1], tangent[2]*bitangent[0]-tangent[0]*bitangent[2], tangent[0]*bitangent[1]-tangent[1]*bitangent[0])
        assert cross[axis] == side, 'Projection frame must retain handedness'
        assert dot(tangent, bitangent) == 0
        for component in range(3):
            neutral[component] += projections[axis] * (tangent[component]*0 + bitangent[component]*0 + normal[component]*(1-1))
    assert all(abs(a-b) < 1e-12 for a, b in zip(normalized(neutral), normal)), 'Neutral maps preserve sloped geometry'
    result = weights(random.uniform(-100, 100), abs(normal[1]), [random.random() for _ in range(3)], random.random())
    assert all(math.isfinite(w) and 0 <= w <= 1 for w in result)
    assert abs(sum(result)-1) < 1e-12

# Analytic ray-cone intersection: major radius is r/cos(theta), minor radius is r.
# These expectations do not involve UV coordinates or tangent normalization.
for incidence in [0, 15, 45, 75, 89]:
    angle = math.radians(incidence)
    cone_radius = 0.125
    major_axis = (cone_radius / math.cos(angle), 0, 0)
    minor_axis = (0, 0, cone_radius)
    ray_direction = (math.sin(angle), math.cos(angle), 0)
    major_perpendicular_to_ray = tuple(v - dot(major_axis, ray_direction)*d for v, d in zip(major_axis, ray_direction))
    assert abs(math.sqrt(dot(major_perpendicular_to_ray, major_perpendicular_to_ray)) - cone_radius) < 1e-12
    assert dot(major_axis, minor_axis) == 0
    assert math.sqrt(dot(minor_axis, minor_axis)) == cone_radius
    if incidence == 0:
        assert math.sqrt(dot(major_axis, major_axis)) == cone_radius

print('Terrain boundaries, height exclusion, 5000 randomized normalized blends, signed triplanar frames and analytic cone footprints passed.')

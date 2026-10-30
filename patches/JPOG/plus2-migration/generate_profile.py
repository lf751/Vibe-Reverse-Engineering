"""Build JPOG's weather bindings without changing installed game files."""

import argparse
import json
from pathlib import Path
import re
import subprocess

WORKSPACE = Path(__file__).resolve().parents[3]
WEATHER = {
    'clear': ('partlyCloudy', ('jpog.clear',)),
    'rain': ('rainstorm', ('jpog.rain',)),
    'storm': ('thunderstorm', ('jpog.storm', 'jpog.tornado')),
    'sandstorm': ('sandstorm', ('jpog.sandstorm',)),
}


def split_arguments(text):
    parts = []
    start = 0
    depth = 0
    for index, char in enumerate(text):
        if char == '(':
            depth += 1
        elif char == ')':
            depth -= 1
        elif char == ',' and depth == 0:
            parts.append(text[start:index].strip())
            start = index + 1
    parts.append(text[start:].strip())
    return parts


def legacy_presets(runtime, revision):
    header = subprocess.check_output(
        ['git', '-C', str(runtime), 'show',
         revision + ':src/dxvk/rtx_render/rtx_weather.h'],
        text=True, encoding='utf-8')
    result = {}
    matches = list(re.finditer(r'^#define WEATHER_PRESET_VALUES_(\w+)\(X\)', header, re.M))
    for index, match in enumerate(matches):
        end = matches[index + 1].start() if index + 1 < len(matches) else len(header)
        fields = {}
        for line in header[match.end():end].splitlines():
            line = line.strip().rstrip('\\').strip()
            if not line.startswith('X('):
                continue
            args = split_arguments(line[2:line.rfind(')')])
            if len(args) != 3:
                raise ValueError(f'Invalid weather macro: {line}')
            kind, name, value = args
            if value.startswith('Vector3('):
                value = value[len('Vector3('):-1]
            value = re.sub(r'(?<=[0-9.])[fF]\b', '', value)
            fields[name] = value
        result[match[1]] = fields
    return result


def settings(runtime):
    text = (runtime / 'submodules/numos/include/numos/settings_fields.inl').read_text(encoding='utf-8')
    result = set()
    starts = [match.start() for match in re.finditer(r'^NUMOS_SETTING\(', text, re.M)]
    for index, start in enumerate(starts):
        end = starts[index + 1] if index + 1 < len(starts) else len(text)
        declaration = text[start:end]
        match = re.search(r'"(rtx\.atmosphere\.[^"]+)"', declaration)
        if match and 'NUMOS_SETTING_NO_SAVE' not in declaration.split('\n', 1)[0]:
            result.add(match[1])
    return result


def generate(config, destination, legacy_runtime, runtime, legacy_ref):
    destination.mkdir(parents=True, exist_ok=True)
    weather_directory = destination / 'weather'
    weather_directory.mkdir(exist_ok=True)
    supported = settings(runtime)
    presets = legacy_presets(legacy_runtime, legacy_ref)
    overrides = {}
    retained = []
    for line in config.read_text().splitlines():
        match = re.match(r'\s*rtx\.weather\.preset\.(\w+)\.\1_(\w+)\s*=\s*(.*)', line)
        if match:
            overrides.setdefault(match[1], {})[match[2]] = match[3].strip()
        elif not re.match(r'\s*rtx\.(?:legacyMaterial\.worldSpaceSecondaryOpacityScale|weather\.transitionSeconds)\s*=', line):
            retained.append(line)
    retained.extend([
        'rtx.legacyMaterial.worldSpaceSecondaryOpacityScale = 0.0009765625',
        'rtx.weather.transitionSeconds = 11',
    ])
    (destination / 'rtx.conf').write_text('\n'.join(retained) + '\n')
    report = {}
    bound_presets = {legacy: (name, ids) for name, (legacy, ids) in WEATHER.items()}
    unknown_presets = overrides.keys() - presets.keys()
    if unknown_presets:
        raise ValueError(f'Unknown authored weather presets: {sorted(unknown_presets)}')
    for legacy, defaults in presets.items():
        name, ids = bound_presets.get(legacy, ('legacy-' + legacy, ()))
        values = defaults | overrides.get(legacy, {})
        mapped = {}
        unmatched = {}
        for key, value in values.items():
            option = 'rtx.atmosphere.' + key
            if key == 'cloudThickness':
                option = 'rtx.atmosphere.cloudDepthMeters'
                value = format(float(value) * 1000, '.9g')
            if option in supported:
                mapped[option] = value
            else:
                unmatched[key] = value
        if not mapped:
            raise ValueError(f'Weather {legacy} has no supported NUMOS fields')
        lines = [
            '# JPOG weather bindings and supported authored atmosphere values.',
            *(['weather.bindings = ' + ', '.join(ids)] if ids else []),
            'weather.transitionSeconds = 11',
            'weather.enabled = true',
            '',
            *(key + ' = ' + value for key, value in mapped.items()),
        ]
        (weather_directory / ('jpog-' + name + '.conf')).write_text('\n'.join(lines) + '\n')
        report[name] = {'sourcePreset': legacy, 'bindings': ids, 'mapped': mapped, 'unmappedLegacyValues': unmatched}
    (destination / 'weather-migration.json').write_text(json.dumps(report, indent=2) + '\n')
    return report


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config', type=Path, required=True,
                        help='Original pre-2.0 rtx.conf, containing authored weather overrides')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--legacy-runtime', type=Path,
                        default=WORKSPACE.parent / 'dxvk-remix_numos')
    parser.add_argument('--legacy-ref', default='ab48b452c',
                        help='Git revision containing the original weather defaults')
    parser.add_argument('--runtime', type=Path, required=True,
                        help='New runtime checkout with pinned NUMOS sources')
    options = parser.parse_args()
    results = generate(options.config, options.output, options.legacy_runtime,
                       options.runtime, options.legacy_ref)
    print(json.dumps({name: {'mapped': len(item['mapped']), 'unmapped': len(item['unmappedLegacyValues'])} for name, item in results.items()}))

# Fails unless every line, and every branch the code itself makes, ran during the tests.
# Reads gcov's `-b -c` output. Branches marked "(throw)" are C++ exception paths the compiler
# adds (the ESP32 build has exceptions off), so they don't count.
import re
import sys

gaps = 0
for path in sys.argv[1:]:
    name = path.rsplit('/', 1)[-1][:-5]
    lines = lines_run = br = br_run = 0
    src = (0, '')
    for l in open(path):
        m = re.match(r'\s*([^:]+):\s*(\d+):(.*)', l)
        if m:
            count, num, code = m.group(1).strip(), int(m.group(2)), m.group(3).strip()
            src = (num, code)
            if count == '-' or num == 0:
                continue
            lines += 1
            if count.startswith(('#####', '=====')):
                gaps += 1
                print(f'  line not run: {name}:{num}: {code}')
            else:
                lines_run += 1
            continue
        b = re.match(r'branch\s+\d+ (taken (\d+)|never executed)( \(throw\))?', l)
        if b and not b.group(3):
            br += 1
            if b.group(2) and int(b.group(2)) > 0:
                br_run += 1
            else:
                gaps += 1
                print(f'  branch not taken: {name}:{src[0]}: {src[1]}')
    print(f'{name:16} lines {lines_run:4}/{lines:<4} branches {br_run:4}/{br:<4}')

print('coverage OK: 100% of lines and branches' if not gaps else f'coverage FAILED: {gaps} gaps')
sys.exit(1 if gaps else 0)

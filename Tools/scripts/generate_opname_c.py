import sys
import tokenize


def main(opcode_py, outfile):
    opcode = {}

    with tokenize.open(opcode_py) as fp:
        code = fp.read()
    exec(code, opcode)
    with open(outfile, 'w') as fobj:
        fobj.write('const char *const opname[] = {\n')
        for name in opcode['opname']:
            fobj.write('    "%s",\n' % name)
        fobj.write('};\n')


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])

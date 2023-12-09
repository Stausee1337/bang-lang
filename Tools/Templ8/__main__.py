import os
import sys

from Templ8.templating import process_file_handle_error
from rich import print as rprint

valid_options = {'-o', '--output', '-h', '--help'}

def print_help(program: str):
    assert len(valid_options) == 4, 'hanlde more opts'

    rprint(f'Usage: {program} \\[options] infile', file=sys.stderr)
    rprint(f'Options:')
    print(f'   --output <file> -o <file>       Specify output file')
    print(f'   --help -h                       Prints this help')

def process_arguments(args: list[str]) -> tuple[str, str]|None:
    assert len(valid_options) == 4, 'hanlde more opts'
    argv = iter(args + [None])

    if (program := next(argv)) is None:
        raise RuntimeError('program name is not available')
    if (option_or_filename := next(argv)) is None:
        rprint(f'{program}: Error: No input file provided', file=sys.stderr)
        return None

    if option_or_filename.startswith('-'):
        option = option_or_filename
        if option not in valid_options:
            rprint(f'{program}: Error: Unknwon option {option!r}', file=sys.stderr)
            return None
        if option in {'-h', '--help'}:
            print_help(program)
            return None
        if (outfile := next(argv)) is None:
            rprint(f'{program}: Error: Expected outfile after option {option!r}', file=sys.stderr)
            return None
        if (infile := next(argv)) is None:
            rprint(f'{program}: Error: No input file provided', file=sys.stderr)
            return None
    else:
        infile = option_or_filename
        if infile.endswith('.templ8'):
            outfile = os.path.splitext(infile)[0]
            if (res := os.path.splitext(outfile))[1] != '':
                outfile = res[0] + '.generated' + res[1]
            else:
                outfile = outfile + '.generated'
        else:
            outfile = infile + '.generated'

    return infile, outfile

def main():
    if (result := process_arguments(sys.argv)) is None:
        exit(1)
    infile, outfile = result

    if not process_file_handle_error(infile, outfile):
        exit(1)

if __name__ == '__main__':
    main()


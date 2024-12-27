import argparse
import yaml
import jinja2
import os

def main():
    template_path = __file__.split("ac_types.py")[0] + "templates/ac_types.h"
    template = jinja2.Template(open(template_path).read())

    parser = argparse.ArgumentParser(description='Generate C++ struct types from a list of types.')
    parser.add_argument("-f", "--files", nargs='+', help="Files to generate types from")

    args = parser.parse_args()

    files = [os.getcwd() + "/" + fname for fname in args.files]
    types = []

    for file in args.files:
        with open(file, 'r') as stream:
            try:
                data = yaml.safe_load(stream)
                types = [(key, data[key]) for key in data]
            except yaml.YAMLError as exc:
                print(exc)

    for t in types:
        print(t)

    print(template.render(files=files, types=types))

if __name__ == '__main__':
    main()
import argparse
import json


class Migrate(object):
    def __init__(self, data):
        self.data = data

    def integrations(self):
        data_integration = self.data.get('integration')
        integration = {
            "How it works": self.data.get("How it works"),
            "How to setup?": self.data.get("How to setup?"),
            "phoneNumberWithLabel": self.data.get("phoneNumberWithLabel"),
            "privacyPolicy": self.data["privacyPolicy"].get("integration"),
            "testedVersionLabel": self.data.get("testedVersionLabel"),
            "testedVersionsLabel": self.data.get("testedVersionsLabel")
        }


def get_args():
    parser = argparse.ArgumentParser("lang-cleanup", description="Cleans lang*.json files")
    parser.add_argument("path", help="Path to file that needs cleaning.")
    return parser.parse_args()


def modify_json(path):
    with open(path, "r") as file:
        translations = json.loads(file.read())

    translations = move_keys(translations)

    with open(path, "w") as file:
        file.write(json.dumps(translations, indent=4, sort_keys=True))


def move_keys(translations):
    return translations


if __name__ == "__main__":
    args = get_args()
    modify_json(args.path)

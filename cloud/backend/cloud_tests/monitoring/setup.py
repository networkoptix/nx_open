#!/usr/bin/env python

from setuptools import setup, find_packages

from version import VERSION

def parse_requirements(filename):
    """ load requirements from a pip requirements file """
    lineiter = (line.strip() for line in open(filename))
    return [line for line in lineiter if line and not line.startswith("#")]

install_requires = parse_requirements('requirements.txt')

setup(name='monitoring_simple',
      version=VERSION,
      description='Simple cloud portal API tests for the purpose of monitoring',
      author='Ivan Vigasin',
      author_email='ivigasin@networkoptix.com',
      url='http://networkoptix.com',
      packages=find_packages(),
      entry_points="""
      [console_scripts]
      monitoring_simple=monitoring_simple.simple:main
      """,
      install_requires=install_requires,      
      include_package_data=True)

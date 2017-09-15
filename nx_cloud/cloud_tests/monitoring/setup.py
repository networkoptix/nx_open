#!/usr/bin/env python

from setuptools import setup, find_packages

from pip.req import parse_requirements
from version import VERSION

install_reqs = parse_requirements('requirements.txt', session=False)

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
      install_requires=[str(ir.req) for ir in install_reqs],
      include_package_data=True)

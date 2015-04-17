#!/usr/bin/python

from setuptools import setup, find_packages
from pip.req import parse_requirements
import sys, os, uuid

from version import version

install_reqs = parse_requirements('requirements.txt', session=uuid.uuid1())
requirements = [str(ir.req) for ir in install_reqs]

setup(name='statserver',
      version=version,
      description="",
      long_description="""\
""",
      classifiers=[], # Get strings from http://pypi.python.org/pypi?%3Aaction=list_classifiers
      keywords='',
      author='Mikhail Uskov',
      author_email='muskov@networkoptix.com',
      url='',
      license='Commercial',
      packages=find_packages(exclude=[
          'ez_setup',
          'examples',
          'tests']),
      scripts=[
          'crashserver_cgi.py',
          'statserver_cgi.py',
          'statserver_fake_report.py',
      ],
      package_data={'nx_statistics': ['sql/*.sql']},
      include_package_data=True,
      zip_safe=False,
      install_requires=requirements,
      entry_points="""
      # -*- Entry points: -*-
      """,
      )

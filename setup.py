from os import path
from setuptools import setup, find_packages

here = path.abspath(path.dirname(__file__))

with open(path.join(here, 'README.rst'), encoding='utf-8') as f:
    long_description = f.read()
with open(path.join(here, 'VERSION'), encoding='utf-8') as f:
    version = f.read().strip()
with open(path.join(here, 'requirements.txt'), encoding='utf-8') as f:
    requirements = f.readlines()

setup(
    name='servo-engine',
    version=version,
    description='Minimalistic server-side session and storage engine',
    long_description=long_description,

    # The project's main homepage.
    url='https://github.com/stan1y/servo',

    download_url='https://github.com/stan1y/servo/archive/%s.tar.gz' % version,

    # Author details
    author='Stan Yudin',
    author_email='stan.yudin@cevo.com.au',

    # Choose your license
    license='MIT',
    packages=find_packages('src', exclude=['contrib', 'docs', 'tests']),
    package_dir={'':'src'},
    package_data={
        '': ['README.rst', 'VERSION', '*.sql'],
    },

    install_requires=requirements,

    entry_points={
        'console_scripts': [
            'servo=servo:main',
        ],
    },

    #test_suite='nose.collector',
    #tests_require=['nose'],
)

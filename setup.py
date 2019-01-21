from setuptools import setup, find_packages

setup(name='pyan',
      setup_requires=['setuptools_scm'],
      use_scm_version=True,
      description="Offline call graph generator for Python 3",
      packages=find_packages(),
      include_package_data=True,
      scripts=['scripts/pyan'],
      zip_safe=False)

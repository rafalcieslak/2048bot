from distutils.core import setup, Extension

module = Extension('bot_core',
                    sources = ['bot_core.cpp'],
                    language = "c++",
                    extra_compile_args=["-std=c++11"])

setup (name = 'bot_core',
       version = '1.0',
       description = 'This is the core module that implements 2048bot\'s searching algorithm',
       ext_modules = [module])
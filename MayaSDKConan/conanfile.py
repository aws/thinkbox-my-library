# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: Apache-2.0
from typing import Any
from conans import ConanFile

import os
import shutil

VALID_MAYA_CONFIGS: dict[tuple[str, str], set[str]] = {
    ('Visual Studio', '16'): { '2022', '2023' },
    ('gcc', '7'): { '2022', '2023' },
    ('gcc', '9'): { '2022', '2023' },
    ('apple-clang', '10.0'): { '2022', '2023' }
}

SETTINGS: dict[str, Any] = {
    'os': ['Windows', 'Linux', 'Macos'],
    'compiler': {
        'Visual Studio': {'version': ['16']},
        'gcc': {'version': ['7', '9']},
        'apple-clang': {'version': ['10.0']}
    },
    'build_type': None,
    'arch': 'x86_64'
}

MAYA_SDK_LIBS: list[str] = [
    'Foundation',
    'OpenMaya',
    'OpenMayaAnim',
    'OpenMayaFX',
    'OpenMayaRender',
    'OpenMayaUI'
]

LIBRARY_EXTENSIONS: dict[str, str] = {
    'Windows': 'lib',
    'Linux': 'so',
    'Macos': 'dylib'
}

LIBRARY_PREFIX: dict[str, str] = {
    'Windows': '',
    'Linux': 'lib',
    'Macos': 'lib'
}

LIBRARY_DIRECTORIES: dict[str, str] = {
    'Windows': 'lib',
    'Linux': 'lib',
    'Macos': 'Maya.app/Conents/MacOS'
}

DEFAULT_MAYA_PATHS: dict[str, str] = {
    'Windows': 'C:/Program Files/Autodesk/Maya{}',
    'Linux': '/usr/autodesk/maya{}',
    'Macos': '/Applications/Autodesk/maya{}'
}


class MayaSDKConan(ConanFile):
    name: str = 'mayasdk'
    version: str = '1.0.0'
    description: str = 'A Conan package containing the Autodesk Maya SDK.'
    settings: dict[str, Any] = SETTINGS
    options: dict[str, Any] = {
        'maya_version': ['2022', '2023'],
        'maya_path': 'ANY'
    }

    def configure(self) -> None:
        if self.options.maya_version == None:
            self.options.maya_version = '2022'

        if self.options.maya_path == None:
            self.options.maya_path = DEFAULT_MAYA_PATHS[str(self.settings.os)].format(self.options.maya_version)

    def validate(self) -> None:
        compiler = str(self.settings.compiler)
        compiler_version = str(self.settings.compiler.version)
        compiler_tuple = (compiler, compiler_version)
        maya_version = str(self.options.maya_version)
        if maya_version not in VALID_MAYA_CONFIGS[compiler_tuple]:
            raise Exception(f'{str(compiler_tuple)} is not a valid configuration for Maya {maya_version}')

    def build(self) -> None:
        # Copy Headers
        build_include_dir = os.path.join(self.build_folder, 'include')
        if os.path.exists(build_include_dir):
            shutil.rmtree(build_include_dir)
        os.makedirs(build_include_dir)
        shutil.copytree(
            os.path.join(str(self.options.maya_path), 'include', 'maya'),
            os.path.join(build_include_dir, 'maya')
        )

        # Copy Libraries
        os_setting = str(self.settings.os)
        build_library_dir = os.path.join(
            self.build_folder,
            'lib'
        )
        if os.path.exists(build_library_dir):
            shutil.rmtree(build_library_dir)
        os.makedirs(build_library_dir)
        for lib in MAYA_SDK_LIBS:
            shutil.copy(
                os.path.join(
                    str(self.options.maya_path),
                    LIBRARY_DIRECTORIES[os_setting],
                    f'{LIBRARY_PREFIX[os_setting]}{lib}.{LIBRARY_EXTENSIONS[os_setting]}'
                ),
                build_library_dir
            )

    def package(self) -> None:
        self.copy('*', dst='bin', src='bin')
        self.copy('*', dst='lib', src='lib')
        self.copy('*', dst='include', src='include')

    def package_info(self) -> None:
        self.cpp_info.libs = self.collect_libs()

    def deploy(self) -> None:
        self.copy('*', dst='bin', src='bin')
        self.copy('*', dst='lib', src='lib')
        self.copy('*', dst='include', src='include')

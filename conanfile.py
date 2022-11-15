# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: Apache-2.0
import os
from typing import Any
from conans import ConanFile, CMake


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

TOOL_REQUIRES: list[str] = [
    'cmake/3.24.1',
    'thinkboxcmlibrary/1.0.0'
]

REQUIRES: list[str] = [
    'thinkboxlibrary/1.0.0',
    'mayasdk/1.0.0',
    'tinyxml2/9.0.0'
]


class ThinkboxMYLibraryConan(ConanFile):
    name: str = 'thinkboxmylibrary'
    version: str = '1.0.0'
    license: str = 'Apache-2.0'
    description: str = 'Shared code for Thinkbox\'s Maya plugins'
    settings: dict[str, Any] = SETTINGS
    requires: list[str] = REQUIRES
    tool_requires: list[str] = TOOL_REQUIRES
    generators: str | list[str] = 'cmake_find_package'
    options: dict[str, Any] = {
        'maya_version': ['2022', '2023']
    }
    
    def configure(self) -> None:
        if self.options.maya_version == None:
            self.options.maya_version = '2022'
        self.options['mayasdk'].maya_version = self.options.maya_version

    def validate(self) -> None:
        if self.options.maya_version != self.options['mayasdk'].maya_version:
            raise Exception('Option \'maya_version\' must be the same as mayasdk')
        compiler = str(self.settings.compiler)
        compiler_version = str(self.settings.compiler.version)
        compiler_tuple = (compiler, compiler_version)
        maya_version = str(self.options.maya_version)
        if maya_version not in VALID_MAYA_CONFIGS[compiler_tuple]:
            raise Exception(f'{str(compiler_tuple)} is not a valid configuration for Maya {maya_version}')

    def build(self) -> None:
        cmake = CMake(self)
        cmake.configure(defs={
            'MAYA_VERSION': self.options.maya_version
        })
        cmake.build()

    def export_sources(self) -> None:
        self.copy('**.h', src='', dst='')
        self.copy('**.hpp', src='', dst='')
        self.copy('**.cpp', src='', dst='')
        self.copy('**.cmake', src='', dst='')
        self.copy('CMakeLists.txt', src='', dst='')
        self.copy('NOTICE.txt', src='', dst='')
        self.copy('LICENSE.txt', src='', dst='')

    def package(self) -> None:
        cmake = CMake(self)
        cmake.install()

        with open(os.path.join(self.source_folder, 'NOTICE.txt'), 'r', encoding='utf8') as notice_file:
            notice_contents = notice_file.readlines()
        with open(os.path.join(self.source_folder, 'LICENSE.txt'), 'r', encoding='utf8') as license_file:
            license_contents = license_file.readlines()
        os.makedirs(os.path.join(self.package_folder, 'licenses'), exist_ok=True)
        with open(os.path.join(self.package_folder, 'licenses', 'LICENSE'), 'w', encoding='utf8') as cat_license_file:
            cat_license_file.writelines(notice_contents)
            cat_license_file.writelines(license_contents)

    def deploy(self) -> None:
        self.copy('*', dst='bin', src='bin')
        self.copy('*', dst='lib', src='lib')
        self.copy('*', dst='include', src='include')

    def package_info(self) -> None:
        self.cpp_info.libs = ["thinkboxmylibrary"]

import io

def patch_file(path, old, new):
    with io.open(path, "r", encoding="utf-8") as f:
        content = f.read()
    if old not in content:
        print(f"SKIP (pattern not found): {path}")
        return False
    content = content.replace(old, new, 1)
    with io.open(path, "w", encoding="utf-8") as f:
        f.write(content)
    print(f"PATCHED: {path}")
    return True

SITE = r"D:\Espressif\python_env\idf6.1_py3.11_env\Lib\site-packages"

patch_file(
    SITE + r"\idf_component_tools\registry\storage_client.py",
    "best_version['download_url'] = join_url(self.storage_url, best_version['url'])\n        best_version['checksums_url'] = join_url(self.storage_url, best_version['checksums'])",
    "best_version['download_url'] = join_url(self.storage_url, best_version['url'])\n        if best_version.get('checksums'):\n            best_version['checksums_url'] = join_url(self.storage_url, best_version['checksums'])\n        else:\n            best_version['checksums_url'] = None",
)

patch_file(
    SITE + r"\idf_component_tools\sources\web_service.py",
    """        storage_client_component = get_storage_client(self.registry_url).component(
            component.name, component.version
        )
        url = storage_client_component['download_url']
        checksums_url = storage_client_component['checksums_url']""",
    """        storage_client_component = get_storage_client(self.registry_url).component(
            component.name, component.version
        )
        url = storage_client_component['download_url']
        checksums_url = storage_client_component.get('checksums_url')""",
)

patch_file(
    SITE + r"\idf_component_tools\sources\web_service.py",
    """            # Download file hashes and copy to cache and download directories
            checksums_path = download_file(checksums_url, tempdir, filename=CHECKSUMS_FILENAME)
            shutil.copy2(checksums_path, component_cache_path)
            shutil.copy2(checksums_path, download_path)""",
    """            # Download file hashes and copy to cache and download directories
            if checksums_url:
                checksums_path = download_file(checksums_url, tempdir, filename=CHECKSUMS_FILENAME)
                shutil.copy2(checksums_path, component_cache_path)
                shutil.copy2(checksums_path, download_path)""",
)

patch_file(
    SITE + r"\idf_component_tools\sources\web_service.py",
    """        storage_client_component = get_storage_client(self.registry_url).component(
            component.name, component.version
        )
        checksums_url = storage_client_component['checksums_url']""",
    """        storage_client_component = get_storage_client(self.registry_url).component(
            component.name, component.version
        )
        checksums_url = storage_client_component.get('checksums_url')
        if not checksums_url:
            return None""",
)
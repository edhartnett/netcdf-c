# UDF Plugin Example

This directory contains a complete example of a user-defined format (UDF) plugin that can be dynamically loaded via RC file configuration.

## Files

- `simple_plugin.c` - Plugin implementation with dispatch table and init function
- `Makefile` - Build script for Unix/Linux/macOS
- `example.ncrc` - Example RC file configuration
- `test_plugin.c` - Test program to verify plugin works

## Building the Plugin

### Unix/Linux/macOS

```bash
make
```

This creates `libsimpleplugin.so` (or `.dylib` on macOS).

### Windows

```bash
cl /LD /I"C:\netcdf\include" simple_plugin.c /link /LIBPATH:"C:\netcdf\lib" netcdf.lib
```

This creates `simpleplugin.dll`.

## Installing the Plugin

Copy the plugin to a permanent location:

```bash
sudo cp libsimpleplugin.so /usr/local/lib/
# Or to a user directory
cp libsimpleplugin.so $HOME/lib/
```

## Configuring RC File

Copy `example.ncrc` to your home directory as `.ncrc`:

```bash
cp example.ncrc ~/.ncrc
```

Edit the file to use the correct path to your plugin library.

## Testing the Plugin

### Automatic Loading (via RC file)

```bash
# Plugin loads automatically during nc_initialize()
./test_plugin
```

### Programmatic Registration

See `test_plugin.c` for an example of registering the plugin programmatically without RC files.

## How It Works

1. When a netCDF application starts, it calls `nc_initialize()`
2. `nc_initialize()` reads RC files and looks for `NETCDF.UDF*.LIBRARY` keys
3. If found, the library is loaded using `dlopen()` (Unix) or `LoadLibrary()` (Windows)
4. The initialization function specified by `NETCDF.UDF*.INIT` is called
5. The init function calls `nc_def_user_format()` to register the dispatch table
6. Files with the specified magic number are automatically handled by the plugin

## Troubleshooting

### Plugin not loading

- Check RC file syntax (both LIBRARY and INIT must be present)
- Verify library path is absolute
- Check file permissions on plugin library
- Enable logging: `export NC_LOG_LEVEL=3`

### Init function not found

- Verify function is not static
- Check symbol exports: `nm -D libsimpleplugin.so | grep init`
- Ensure function name matches INIT key in RC file

### ABI version mismatch

- Recompile plugin against current netCDF-C headers
- Check `NC_DISPATCH_VERSION` in your code matches library

## See Also

- User documentation: `docs/user_defined_formats.md`
- Developer guide: `docs/udf_plugin_development.md`
- Dispatch architecture: `docs/dispatch.md`

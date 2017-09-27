## What is this?
A nginx module enabling fetching of files that are stored in zipped archives.

## Directives
### unzip
- **syntax**: `unzip [on|off]`
- **default**: `off`
- **context**: http, server, location

activate the module


### unzip\_archive
- **syntax**: `unzip_archive uri`
- **default**: none
- **context**: location

points to the zipped file, required


### unzip\_path
- **syntax**: `unzip_path uri`
- **default**: none
- **context**: location

file to be extracted from the zipped file, required


### unzip\_path\_encoding
- **syntax**: `unzip_path_encoding ENCODING`
- **default**: `""`
- **context**: http, server, location

character encoding for path in zip.


### unzip\_autoindex
- **syntax**: `unzip_autoindex [on|off]`
- **default**: `off`
- **context**: http, server, location

enable autoindex feature.
Slow for archives with many files.


### unzip\_nocase
- **syntax**: `unzip_nocase [always|fallback|disable]`
- **default**: `disable`
- **context**: http, server, location

Ignore case distinctions.
Slow for archives with many files.


## Example
```
location ~ ^/(.+?\.zip)/(.*)$ {
    unzip on;
    unzip_archive "$document_root/$1";
    unzip_path "$2";
    unzip_autoindex on;
    unzip_nocase fallback;
    unzip_path_encoding "SHIFT_JIS";
}
```


## issue
- On musl libc, cannot set unzip\_path\_encoding like "SHIFT\_JIS". This is [musl libc limitation](http://wiki.musl-libc.org/wiki/Functional_differences_from_glibc).

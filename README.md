## What is this?
A nginx module enabling fetching of files that are stored in zipped archives.

## Nginx configuration example

* unzip: flag activating the module
* unzip_archive: points to the zipped file
* unzip_path: file to be extracted from the zipped file
* unzip_autoindex: autoindexing zip file (slow)

```
location ~ ^/(.+?\.zip)/(.*\.jpg)$ {
    add_header Content-Type image/jpeg;
    unzip on;
    unzip_archive "$document_root/$1";
    unzip_path "$2";
}

location ~ ^/(.+?\.zip)/(.*)$ {
    add_header Content-Type text/html;
    unzip_archive "$document_root/$1";
    unzip_path "$2";
    unzip_autoindex on;
}
```


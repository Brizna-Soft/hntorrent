UPDATE setting SET value = '"https://github.com/Brizna-Soft/brizna-descargas"', default_value = '"https://github.com/Brizna-Soft/brizna-descargas"' WHERE key = 'update_checks.url';
UPDATE setting SET value = 'true' WHERE key = 'update_checks.enabled';
UPDATE setting SET value = NULL WHERE key = 'update_checks.ignored_version';

<?php

require(__DIR__."/passwordstore/htpasswd.php");
require(__DIR__."/datastore/pdo.php");

$db = new \PDO('mysql:dbname=sso;host=db.example.com.', "user", "password");
$db->setAttribute(\PDO::ATTR_ERRMODE, \PDO::ERRMODE_EXCEPTION);
$db->setAttribute(\PDO::ATTR_EMULATE_PREPARES, false);

\sso\config::$passwordstore = new \sso\PasswordStore\htpasswd('/etc/apache2/passwords.htpasswd');
\sso\config::$datastore = new \sso\DataStore\PDO($db);

// This can be uset to limit the aquisition of tokens to certain domains & URLs
// \sso\config::$token_whitelist_pattern = '/^https:\/\/.*/';

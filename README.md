# A simple SSO Portal

This is a Single Sign On (SSO) portal written in PHP.

In the `config.php` file, you can configure where and how to save sessions, and how to check or save passwords.

If the portal is opened normally, you can log in, check the sessions, log out of the current session, log out all your sessions,
and change your password.

If the page is opened with the `renew-token` GET parameter set, and the `referer` GET or POST parameter is also set, a token will be checked,
created, or renewed, and the existing or a new token is sent back, to the URL origin derived from the `referer`, with the location `/.well-known/dpa-sso/`,
and the GET parameters `token` and `location` appended. Additionally, the headers `X-User` and `X-Token` are set. The HTTP `Referer` header
was used initially, but prooved unreliable.  
The token is unique to the origin, and distinct from the session token. If `renew-token` doesn't contain a valid token *and* there is no known
session, the login page will be shown and the HTTP status will be 401. If the token is known and valid, the session (optional, if present)
matches, and the token isn't old, the existing token is returned. Otherwise, a new token is returned, if a session is specified, it is for the
specified session, otherwise, it is for the same session as the old token. `Referer` can be overwritten using a get parameter named "referer".

There is also the `/permission/` endpoint. Given a *token*, a new *permission token* can be generated, that can be checked using this endpoint.
The new *permission token* starts with a list of permissions followed by the sha256 of the permission list and the *token*. Everything is seperated by a single space.
The token can be checked by passing it to the `/permission/` endpoint using the `token` POST parameter. The user also needs to be passed using the `user` POST parameter.
That is mainly so only the tokens of that user need to be checked if they were used to generate the permission token. The `/permission/` returns a JSON
containing the `origin` of the token and some other infos if it is valid. It returns null and status 403 otherwise. This endpoint does not require authentification.
A *permission token* can (currently) not be used in places where a regular *token* can be used.  
Which origins can have which permissions needs to be specified in the config. While generating *permission tokens* is always possible, it will return a json
containing with a `permission` field, which contains the subset of allowed permissions. If there are none, the token is treated like an invalid token.
The fields are also returned as HTTP Headers, prefixed with `X-`.

The usecase for *permission token* is to allow a webbapplication to access other applications. Suppose there was a web mail application at origin `https://mail.example.com`.
Suppose a user logged in at the SSO portal, and the application got a *token* using which the user is logged in, let's suppose it is `abc123`.
Now, the web mail application may need to access the mail server (smtp and/or imap service). So it will generate a permission, for example `smtp imap 7609517ee082128699fa209eeaa058203e600371071bcad14594036b491fa51a`.
The mail server / it's smtp / imap applications, can then be configured to accept the username and that *permission token*, and use the `/permission/` endpoint to validate it.
See the `libpam-dpa-sso` branch for an example to configure postfix and dovecot to do so using pam. The config file of this portal would have to contain an entry like the following:
```
\sso\config::$permission_map = [
  "https://mail.example.com" => ["smtp","imap"],
];
```
For completeness sake, here is what a roundcube webmail config should contain:
```
$config['smtp_user'] = '%u';
$config['smtp_pass'] = '%p';
$_SERVER['PHP_AUTH_PW'] = 'smtp imap '.hash('sha256', 'smtp imap '.$_SERVER['PHP_AUTH_PW']);
$config['plugins'] = ['http_authentication'];
```
Note, that is when using the `libapache2-mod-auth-dpa-sso` httpd module, with the `AuthDPASSOFakeBasicAuth` set to `user-password`.
It can also be set up using `sso-middleware-php` instead, but this currently isn't documented.

See also the other branches, there is an apache2 httpd auth module at the `libapache2-mod-auth-dpa-sso`. There is also a PHP counterpart at `sso-middleware-php`.

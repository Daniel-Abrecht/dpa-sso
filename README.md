# A simple SSO Portal

This is a Single Sign On (SSO) portal written in PHP.

In the `config.php` file, you can configure where and how to save sessions, and how to check or save passwords.

If the portal is opened normally, you can log in, check the sessions, log out of the current session, log out all your sessions,
and change your password.

If the page is opened with the `renew-token` GET parameter set, and the HTTP `Referer` header is also set,
a token will be checked, created, or renewed, and the existing or a new token is sent back, to the URL origin derived from the HTTP `Referer`,
with the location `/.well-known/dpa-sso/`, and the GET parameters `token` and `location` appended. Additionally, the headers `X-User` and `X-Token` are set.
The token is unique to the origin, and distinct from the session token. If `renew-token` doesn't contain a valid token *and* there is no known
session, the login page will be shown and the HTTP status will be 401. If the token is known and valid, the session (optional, if present)
matches, and the token isn't old, the existing token is returned. Otherwise, a new token is returned, if a session is specified, it is for the
specified session, otherwise, it is for the same session as the old token. `Referer` can be overwritten using a get parameter named "referer".

There is also the `/permission/` endpoint. Given a *token*, a new *permission token* can be generated, that can be checked using this endpoint.
The new *permission token* starts with the permission name followed by a `/`, followed by the sha256 of the permission name followed by a space and the token.
The token can be checked by passing it to the `/permission/` endpoint using the `token` GET parameter. The user also needs to be passed using the `user` GET parameter.
That is mainly so only the tokens of that user need to be checked if they were used to generate the permission token. The `/permission/` returns a JSON
containing the `origin` of the token and some other infos if it is valid. It returns null and status 403 otherwise. This endpoint does not require authentification.
A *permission token* can (currently) not be used in places where a regular *token* can be used.  
Which origins are allowed to generate *permission tokens* for which origin needs to be specified in the config. While generating them is always possible,
this endpoint will check the origin and treat them as invalid otherwise.

See also the other branches, there is an apache2 httpd auth module at the `libapache2-mod-auth-dpa-sso`. There is also a PHP counterpart at `sso-middleware-php`.

# A simple SSO Portal

This is a Single Sign On (SSO) portal written in PHP.

In the `config.php` file, you can configure where and how to save sessions, and how to check or save passwords.

If the portal is opened normally, you can log in, check the sessions, log out of the current session, log out all your sessions,
and change your password.

If the page is opened with the `renew-token` GET parameter set, and the HTTP `Referer` header is also set,
a token will be checked, created, or renewed, and the existing or a new token is sent back, to the URL origin derived from the HTTP `Referer`,
with the location `/sso/`, and the GET parameters `user`, `token` and `location` appended. The token is unique to the origin, and distinct
from the session token. If `renew-token` doesn't contain a valid token *and* there is no known session, the login page will be shown and the
HTTP status will be 401. If the token is known and valid, the session (optional, if present) matches, and the token isn't old, the existing
token is returned. Otherwise, a new token is returned, if a session is specified, it is for the specified session, otherwise, it is for the
same session as the old token.

See also the other branches.

# auth_dpa-sso apache2 httpd module

It can be used similar to other auth modules:
```
AuthType dpa-sso
AuthDPASSOLocation https://sso.example.com/
AuthDPASSOFakeBasicAuth user
```

The `AuthDPASSOLocation` directive schould point to the SSO portal.

The `AuthDPASSOFakeBasicAuth` is for adding a basic auth header, which can then be used by CGI scripts, PHP, etc.
It accepts the values `Off`, `On` / `user` (this only sets the user), and `user-password`, which sets the token as the password.
When proxying requests, be careful whom you give access to which tokens, and to transfer them over a secure connection.

When this module is loaded, it will process requests to `/.well-known/dpa-sso/`. This endpoint sets the `dpa-sso-token` cookie if
not already set, and redirects to another page on the same origin by using the `location` GET field if present. This is ensured
by requiring the location to start with `/`. This endpoint is unaffected by auth modules and such things. The token is not checked
here, it is only set. It will be checked for each request to locations requiring authentication by this auth module.

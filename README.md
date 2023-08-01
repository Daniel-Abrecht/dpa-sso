# dpa-sso PAM module

This is a pam auth module for verifying a permission token derived from a token from the dpa-sso portal.
You have to specify the location of the permission endpoint of the dpa-sso portal, and a colon separated list of permissions.

# How to configure smtp & imap login for dovecot and postfix

When authenticating over imap, dovecot presents itself to pam as imap service, thus the config file is `/etc/pam.d/imap`.
For postfix over smtp, it's smtp, and the config file is `/etc/pam.d/smtp`.
When these files are not present, pam uses `/etc/pam.d/other`.

As a first step, if `/etc/pam.d/smtp` or `/etc/pam.d/imap` is not present, create them by copying `/etc/pam.d/other`.

To `/etc/pam.d/imap` you can add the following line before the other lines starting with `auth`:
```
auth sufficient pam_dpa-sso.so url=https://sso.abrecht.li/permission/ permission=imap
```

To `/etc/pam.d/smtp` you can add the following line before the other lines starting with `auth`:
```
auth sufficient pam_dpa-sso.so url=https://sso.abrecht.li/permission/ permission=smtp
```

The `auth sufficient` means, that if no other auth module hes denyed access yet, and this module succeeds,
all other auth modules are ignored and the user is authenticated. If this module fails auth, it means that's ignored,
and the following auth modules are considered.

On my devuan server, my complete `/etc/pam.d/smtp` config looks as follows:
```
auth sufficient pam_dpa-sso.so url=https://sso.abrecht.li/permission/ permission=smtp

@include common-auth
@include common-account
@include common-password
@include common-session
```
But how it should look like on your server will depend on your distribution and preferences.

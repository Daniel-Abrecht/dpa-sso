<?php namespace sso;

interface PasswordStore {
  function check(string $user, string $password) : bool;
  function save(string $user, string $password) : bool;
}

class Session {
  public string $token;
  public \DateTimeInterface $created;
  public \DateTimeInterface $last_update;
  public ?int $valid_for = null;
  public string $user;
  public ?string $user_agent;
}

class Authorization {
  public string $origin;
  public string $token;
  public string $session;
  public string $stale;
  public \DateTimeInterface $created;
  public \DateTimeInterface $last_update;
  public ?int $valid_for = null;
}

interface DataStore {
  function loadSessionOfUser(string $user) : array;
  function deleteSessionOfUser(string $user) : void;
  function loadSession(string $token) : ?Session;
  function saveSession(Session $session) : void;
  function deleteSession(Session $session) : void;
  function loadAuthorizationOfSession(Session|string $s) : array;
  function loadAuthorization(string $origin, string $token) : ?Authorization;
  function saveAuthorization(Authorization $auth) : void;
  function deleteAuthorization(Authorization $auth) : void;
}

class config {
  public static DataStore $datastore;
  public static PasswordStore $passwordstore;
  public static string $token_whitelist_pattern = '/^https:\/\/.*/';
}

function create_token(){
  return hash('sha256', file_get_contents("/dev/urandom", false, null, 0, 1024));
}

<?php namespace sso\DataStore;

require_once(__DIR__."/../sso-base.php");

class PDO implements \sso\DataStore {
  function __construct(public \PDO $pdo){}
  private static function convertSession($r,$s=null){
    if(!$s) $s = new \sso\Session();
    $s->token = $r['token'];
    $s->created = new \DateTimeImmutable($r['created']);
    $s->last_update = new \DateTimeImmutable($r['last_update']);
    $s->valid_for = $r['valid_for'];
    $s->user = $r['user'];
    $s->user_agent = $r['user_agent'];
    return $s;
  }
  function loadSession(string $token) : ?\sso\Session {
    $sth = $this->pdo->prepare("SELECT `token`, `created`, `last_update`, `valid_for`, `user`, `user_agent` FROM `valid_session` WHERE `token`=:token LIMIT 1");
    $sth->execute(['token'=>$token]);
    $r = $sth->fetch();
    if(!$r) return null;
    return self::convertSession($r);
  }
  function saveSession(\sso\Session $session) : void {
    if(!isset($session->token) || $session->token === '')
      $session->token = \sso\create_token();
    $sth = $this->pdo->prepare("INSERT INTO `session` (`token`, `valid_for`, `user`, `user_agent`) VALUES (:token, :valid_for, :user, :user_agent) ON DUPLICATE KEY UPDATE `token`=VALUES(`token`), `valid_for`=VALUES(`valid_for`), `user`=VALUES(`user`), `user_agent`=VALUES(`user_agent`)");
    $sth->execute([
      'token' => $session->token,
      'valid_for' => $session->valid_for,
      'user' => $session->user,
      'user_agent' => $session->user_agent,
    ]);
  }
  function deleteSession(\sso\Session $session) : void {
    $sth = $this->pdo->prepare("DELETE FROM `session` WHERE `token`=:token");
    $sth->execute(['token'=>$session->token]);
  }
  function loadSessionOfUser(string $user) : array {
    $sth = $this->pdo->prepare("SELECT `token`, `created`, `last_update`, `valid_for`, `user`, `user_agent` FROM `valid_session` WHERE `user`=:user");
    $sth->execute(['user'=>$user]);
    return array_map(fn($r)=>self::convertSession($r), $sth->fetchAll());
  }
  function deleteSessionOfUser(string $user) : void {
    $sth = $this->pdo->prepare("DELETE FROM `valid_session` WHERE `user`=:user");
    $sth->execute(['user'=>$user]);
  }
  private static function convertAuthorization($r,$s=null){
    if(!$s) $s = new \sso\Authorization();
    $s->origin = $r['origin'];
    $s->token = $r['token'];
    $s->session = $r['session_token'];
    $s->created = new \DateTimeImmutable($r['created']);
    $s->last_update = new \DateTimeImmutable($r['last_update']);
    $s->valid_for = $r['valid_for'];
    $s->stale = $r['stale'];
    return $s;
  }
  function loadAuthorizationOfSession(\sso\Session|string $s) : array {
    $sth = $this->pdo->prepare("SELECT `origin`, `token`, `session_token`, `created`, `last_update`, `valid_for`, `stale` FROM `valid_authorization` WHERE `session_token`=:token");
    $sth->execute(['token'=>($s instanceof \sso\Session ? $s->token : $s)]);
    $rs = $sth->fetchAll();
    foreach($rs as &$r)
      $r = self::convertAuthorization($r);
    return $r;
  }
  function loadAuthorization(string $origin, string $token) : ?\sso\Authorization {
    $sth = $this->pdo->prepare("SELECT `origin`, `token`, `session_token`, `created`, `last_update`, `valid_for`, `stale` FROM `valid_authorization` WHERE `origin`=:origin AND `token`=:token LIMIT 1");
    $sth->execute(['origin'=>$origin,'token'=>$token]);
    $r = $sth->fetch();
    if(!$r) return null;
    return self::convertAuthorization($r);
  }
  function saveAuthorization(\sso\Authorization $auth) : void {
    if(!isset($auth->token) || $auth->token === '')
      $auth->token = \sso\create_token();
    $sth = $this->pdo->prepare("INSERT INTO `authorization` (`origin`, `token`, `session`, `valid_for`) VALUES (:origin, :token, (SELECT `id` FROM `valid_session` WHERE `token`=:session), :valid_for) ON DUPLICATE KEY UPDATE `token`=VALUES(`token`), `valid_for`=VALUES(`valid_for`)");
    $sth->execute([
      'origin' => $auth->origin,
      'token' => $auth->token,
      'session' => $auth->session,
      'valid_for' => $auth->valid_for
    ]);
  }
  function deleteAuthorization(\sso\Authorization $auth) : void {
    $sth = $this->pdo->prepare("DELETE FROM `authorization` WHERE `origin`=:origin AND `token`=:token");
    $sth->execute(['origin'=>$auth->origin,'token'=>$auth->token]);
  }
}

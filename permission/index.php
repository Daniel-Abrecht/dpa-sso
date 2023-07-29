<?php

 error_reporting(E_ALL);
 ini_set("display_errors", 1);

require(__DIR__."/../sso.php");

if(!@$_SERVER["HTTPS"]){
  header("HTTP/1.1 400 Bad Request");
  die();
}

$user = $_GET['user'] ?? null;
@list($permission, $token) = explode('/', $_GET['token']??'', 2);

if(!$user || !$permission || !$token){
  header("HTTP/1.1 400 Bad Request");
  die();
}

$allowed_origins = [];
foreach(\sso\config::$permission_map as $origin => $permissions)
  if(in_array($permission, $permissions, true))
    $allowed_origins[] = $origin;

$authorization = null;
foreach(\sso::loadSessionOfUser($user) as $session)
foreach(\sso::loadAuthorizationOfSession($session) as $auth){
  if(!in_array($auth->origin, $allowed_origins, true))
    continue;
  if($token !== hash('sha256', 'mail '.$auth->token))
    continue;
  $authorization = $auth;
  break 2;
}

header("Content-Type: application/json");

if(!$authorization){
  header("HTTP/1.1 403 Forbidden");
  echo 'null';
  die();
}

echo JSON_encode([
  'origin' => $authorization->origin,
  'user' => $user,
  'permission' => $permission,
  'created' => $authorization->created->format(DateTime::ATOM),
  'last_update' => $authorization->last_update->format(DateTime::ATOM),
]);

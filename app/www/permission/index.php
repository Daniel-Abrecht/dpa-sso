<?php
require(__DIR__."/../config.php");
require(LIB_DIR."/sso.php");

if(!@$_SERVER["HTTPS"]){
  header("HTTP/1.1 400 Bad Request");
  die();
}

$user = $_POST['user'] ?? null;
$perms = explode(' ', $_POST['token']??'');
$token = array_pop($perms);

if(!$user || !$perms || !$token){
  header("HTTP/1.1 400 Bad Request");
  die();
}

$allowed_origins = [];
foreach(\sso\config::$permission_map as $origin => $permissions)
  if(array_intersect($perms, $permissions))
    $allowed_origins[] = $origin;

$authorization = null;
foreach(\sso::loadSessionOfUser($user) as $session)
foreach(\sso::loadAuthorizationOfSession($session) as $auth){
  if(!in_array($auth->origin, $allowed_origins, true))
    continue;
  if($token !== hash('sha256', implode(' ',$perms).' '.$auth->token))
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

$permissions = array_intersect($perms, \sso\config::$permission_map[$authorization->origin]);
$data = [
  'origin' => $authorization->origin,
  'user' => $user,
  'permission' => $permissions,
  'token' => implode(' ',$permissions).' '.hash('sha256', implode(' ',$permissions).' '.$authorization->token),
];

foreach($data as $key => $value){
  $k = 'X-'.str_replace('_','-',ucfirst($key)).': ';
  if(is_array($value)){
    foreach($value as $v)
      header($k.($v??''), false);
  }else{
    header($k.($value??''));
  }
}

echo JSON_encode($data);

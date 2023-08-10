<?php
require(__DIR__."/config.php");
require(LIB_DIR."/sso.php");

if(!@$_SERVER["HTTPS"]){
  header("HTTP/1.1 400 Bad Request");
  die();
}

if($_SERVER['REQUEST_METHOD'] !== 'GET' && $_SERVER['REQUEST_METHOD'] !== 'HEAD')
  header("Cache-Control: no-store", true);

$self_origin = 'https://' . $_SERVER['HTTP_HOST'] === ($_SERVER['HTTP_ORIGIN']??null);

$referer_origin = null;
$referer_location = null;
$referer = $_REQUEST['referer'] ?? null;
if( !$referer
 || !str_starts_with($referer, 'https://')
 ||  str_starts_with($referer, 'https://'.$_SERVER['HTTP_HOST'])
 || !preg_match(\sso\config::$token_whitelist_pattern, $referer)
 || !preg_match('/^(https:\/\/[^\/]+)(.*)/', $referer, $referer_origin)
) $referer = null;
if($referer_origin){
   $referer_location = $referer_origin[2];
   $referer_origin = $referer_origin[1];
}

$renew_token = $_REQUEST['renew-token'] ?? null;
if($renew_token !== null && !$referer)
  die("Missing HTTP referer");

$login = null;
function saveSession(){
  global $login;
  if(!$login) return;
  if(@$_SERVER['HTTP_USER_AGENT'])
    $login->user_agent = @$_SERVER['HTTP_USER_AGENT'];
  \sso::save($login);
  setcookie("sso-session", $login->token, [
    'expires' => time()+7*24*60*60,
    'path' => '/',
    'secure' => true,
    'httponly' => true,
    'samesite' => 'None'
  ]);
}

if(isset($_COOKIE['sso-session']))
  $login = \sso::load(\sso\Session::class, $_COOKIE['sso-session']);

if($renew_token !== null){
  $token_to_renew = \sso::load(\sso\Authorization::class, $referer_origin, $renew_token);
  if($token_to_renew && $login && $token_to_renew->session != $login->token)
    $token_to_renew = null;
  if($token_to_renew && !$login)
    $login = \sso::load(\sso\Session::class, $token_to_renew->session);
  if(!$login)
    $token_to_renew = null;
  if($token_to_renew && $token_to_renew->stale)
    $token_to_renew = null;
}

if($login && isset($_POST['logout-all']) && $self_origin){
  \sso::logout($login->user);
  $login = null;
}

if($login && isset($_POST['logout']) && $self_origin){
  \sso::delete($login);
  $login = null;
}

if(!$login){
  $user = null;
  $password = null;
  $mode = null;
  if(isset($_SERVER['PHP_AUTH_USER'])){
    $user = $_SERVER['PHP_AUTH_USER'];
    $password = $_SERVER['PHP_AUTH_PW'];
    $mode = @$_GET['mode'];
  }else if(@$_POST['user']){
    $user = @$_POST['user'];
    $password = @$_POST['password'];
    $mode = @$_POST['mode'];
  }
  $valid_for = strtotime('tomorrow') - time();
  if($mode === 'inf') $valid_for = null;
  if($mode === '1h') $valid_for = 60 * 60;
  if($user && \sso::check_password($user, $password)){
    $login = new \sso\Session();
    $login->user = $user;
    $login->valid_for = $valid_for;
    saveSession();
  }
}

if(!$login){
  header('HTTP/1.0 401 Unauthorized');
  if(isset($_GET['basic']))
    header('WWW-Authenticate: Basic realm="SSO abrecht.li"');
  require(LIB_DIR."/login.php");
  exit();
}

if(!@$token_to_renew)
  saveSession();

if($referer && $renew_token !== null){
  $auth = $token_to_renew ?? null;
  if(!$auth){
    $auth = new \sso\Authorization();
    $auth->origin = $referer_origin;
    $auth->session = $login->token;
  }
  \sso::save($auth);
  header("HTTP/1.1 303 See Other");
  header("X-User: ".$login->user);
  header("X-Token: ".$auth->token);
  header("Cache-Control: no-store", true);
  header("Content-Type: application/json", true);
  header("Location: $referer_origin/.well-known/dpa-sso/?token=".rawurlencode($auth->token)."&location=".rawurlencode($referer_location));
  echo JSON_encode(['user'=>$login->user]);
  exit();
}else{
  if($_SERVER['REQUEST_METHOD'] !== 'GET' && $_SERVER['REQUEST_METHOD'] !== 'HEAD'){
    // This will make the browser follow up the request with a get request
    // This will remove the previous request from the history, if for example, it was the login
    header("HTTP/1.1 303 See Other");
    header("Location: #");
    exit();
  }else if($_SERVER['REQUEST_METHOD'] !== 'HEAD'){
    require(LIB_DIR."/main.php");
  }
}

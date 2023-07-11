<?php namespace sso;

class config {
  public static string $portal_url;
  public static string $origin;
};
\sso\config::$origin = 'https://' . $_SERVER["HTTP_HOST"];

require_once(__DIR__."/config.php");

class sso {
  public static ?string $user = null;

  public static function init(){
    $token = null;

    if($_COOKIE['sso-token']??false){
      if(file_get_contents(
        \sso\config::$portal_url . '?renew-token=' . rawurlencode($_COOKIE['sso-token']),
        false, stream_context_create(['http'=>[
          'method' => "HEAD",
          "header" => "Referer: https://" . $_SERVER["HTTP_HOST"] . "\r\n",
          'follow_location' => 0
        ]])
      ) !== false){
        $status = explode(' ', array_shift($http_response_header), 3)[1];
        $result = [];
        foreach($http_response_header as $val){
          list($key, $value) = explode(':', $val, 2);
          $key = trim($key);
          $value = trim($value);
          $result[$key] = $value;
        }
        if($status === '303' && @$result['X-User'] && @$result['X-Token']){
          self::$user = $result['X-User'];
          $token = $result['X-Token'];
        }
      }
    }

    if(!$token){
      header_remove("Expires");
      header_remove("E-Tag");
      header("HTTP/1.1 303 See Other");
      header("Location: " . \sso\config::$portal_url . '?renew-token&referer=' . rawurlencode(\sso\config::$origin.$_SERVER['REQUEST_URI']));
      exit();
    }

    setcookie("sso-token", $token, [
      'expires' => time()+1*60*60,
      'path' => '/',
      'secure' => true,
      'httponly' => true,
      'samesite' => 'None'
    ]);
  }

}

sso::init();

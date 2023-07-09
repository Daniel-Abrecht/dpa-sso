<?php namespace sso\PasswordStore;

require_once(__DIR__."/../sso-base.php");

class htpasswd implements \sso\PasswordStore {
  function __construct(public string $file){}
  function check(string $user, string $password) : bool {
    $file = @file($this->file);
    if(!$file) return false;
    foreach($file as $line){
      list($u, $hash) = @explode(':', trim($line), 2);
      if($user === $u)
        return password_verify($password, $hash);
    }
    return false;
  }
  function save(string $user, string $password) : bool {
    if(str_contains($user,':'))
      throw new Exception("Username can't contain ':'");
    $hash = password_hash($password, PASSWORD_BCRYPT);
    $content = @file_get_contents($this->file);
    if(!$content) $content = '';
    $lines = array_filter(explode('\n',$content), fn($value)=>$value!=='');
    $found = false;
    foreach($lines as &$line){
      list($u, $h) = @explode(':', $line, 2);
      if($u !== $user)
        continue;
      $line = "$user:$hash";
      $found = true;
    }
    if(!$found)
      $lines[] = "$user:$hash";
    $lines[] = '';
    $content = implode("\n",$lines);
    if(($tmp=@tempnam(null, null)) === false)
      return false;
    if(file_put_contents($tmp, $content) === false)
      return false;
    if(!rename($tmp, $this->file))
      return false;
    return true;
  }
}

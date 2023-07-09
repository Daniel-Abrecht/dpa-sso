<?php require("header.php") ?>
<form method="post" action="." style="float: right;"><input type="submit" name="logout-all" value="Alle Ausloggen" class="btn"/></form>
<h1>Sessions</h1>
<table>
  <thead>
    <tr>
      <th class="s">Von</th>
      <th class="s">Bis</th>
      <th class="s">Letzte Nutzung</th>
      <th>User Agent</th>
    </tr>
  </thead>
  <tbody><?php
foreach(\sso::loadSessionOfUser($login->user) as $session){
  echo "<tr>\n";
  echo "  <td class=\"s\">\n";
    echo $session->created->format("Y-m-d H:i:s T");
  echo "  </td>\n";
  echo "  <td class=\"s\">\n";
    if($session->valid_for){
      $until = DateTimeImmutable::createFromInterface($session->created)->modify('+'.$session->valid_for.' seconds');
      echo $until->format("Y-m-d H:i:s T");
    }else{
      echo "Permanent";
    }
  echo "  </td>\n";
  echo "  <td class=\"s\">\n";
    echo $session->last_update->format("Y-m-d H:i:s T");
  echo "  </td>\n";
  echo "  <td>\n";
    echo htmlentities($session->user_agent);
  echo "  </td>\n";
  echo "</tr>\n";
}
?>
  </tbody>
</table>
<?php require("footer.php") ?>

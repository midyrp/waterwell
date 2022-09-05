<?php
#
#  curl -s http://<host>/datareader.php?datasource=<name> | jq
#  data will be taken from: <name>*.dat
#

$maxDataAge= 86400;
$DATAPATH="/var/www/html";

if ( ! isset($_GET['datasource']) ) {
  # No datasource specified
  header($_SERVER["SERVER_PROTOCOL"]." 404 Not Found", true, 404);
  echo "[{\"status\":\"error\",";
  echo "\"reason\":\"no source specified\"";
  echo "}]";
} else {
  # Get most recent record
  $req = $_GET['datasource'];
  exec("cat $req*.dat | sort | tail -n 1 ", $myout);
  if ( count($myout) == 0 ){
    # No record found
    header($_SERVER["SERVER_PROTOCOL"]." 404 Not Found", true, 404);
    echo "[{\"status\":\"error\",";
    echo "\"reason\":\"no data\"";
    echo "}]";
  } else {
    # Check record age
    $tmpStr = json_decode($myout[0])->timestamp;
    $tmpStr[8]=' ';
    $recTime = strtotime($tmpStr);
    $currentTime=strtotime(date('Ymd His'));
    if ( $currentTime-$recTime > $maxDataAge ) {
      # Data to old
      echo "[{\"status\":\"error\",";
      echo "\"reason\":\"data to old\"";
      echo "}]";
    } else {
      # We have data to offer !!
      echo "[{\"status\":\"ok\",\"data\":";
      foreach ($myout as $dat){
        echo $dat;
      }
      echo "}]";
    }
  }
}
?>

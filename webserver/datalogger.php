<?php
# http://192.168.37.55/datalogger.php?dataname=Discworld&reading=10
# 
$DATAPATH="/var/www/html";

$mytimestamp=date('Ymd');

if ( ! isset($_GET['datasource']) ) {
  header($_SERVER["SERVER_PROTOCOL"]." 404 Not Found", true, 404);
  echo "NACK<br>No source specified";
} else {
  $myfile = fopen($DATAPATH . "/" . $_GET['datasource'] . "-" . $mytimestamp . ".dat" , "a") or	die("Failed to create file"); 
  fwrite($myfile, "{");
  $cnt=0;
  foreach($_GET as $key=>$value){
    if ( $cnt++ >0 ){
      fwrite($myfile, ",");
    }
    fwrite($myfile, "\"" . $key . "\":\"" . $value . "\"");
  }
  fwrite($myfile, "}\n");
  fclose($myfile);
  echo "ACK<br>";
  echo $DATAPATH . "/" . $_GET['datasource'] . "<br>";
}
?>

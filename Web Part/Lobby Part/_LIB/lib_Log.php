<?php


$cnf_SystemLog_URL = "http://172.16.2.2/LOG/LogSystem.php";
$cnf_GameLog_URL = "http://172.16.2.2/LOG/LogGame.php";
$cnf_Profileing_URL = "http://172.16.2.2/LOG/LogProfiling.php";
$cnf_LOG_LEVEL = 3; 

$cnf_PROFILING_LOG_RATE = 1;

include "GameLog.php";
include "lib_http_async.php";

function LOG_System($AccountNo, $Action, $Message = "", $Level = 5)
{
	global $cnf_SystemLog_URL;

	$postField = array("AccountNo" => $AccountNo, "Action" => $Action, "Message" => $Message, "Level" => $Level);
	
	$Response = http_request_async($cnf_SystemLog_URL, $postField, "POST");
}
?>
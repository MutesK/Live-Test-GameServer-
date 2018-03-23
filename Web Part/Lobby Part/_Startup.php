<?php


$g_Account = 0;

include "_LIB/lib_Common.php";
include "_LIB/lib_Log.php";
include "_LIB/lib_ErrorHandler.php";
include "_LIB/lib_Profiling.php";

global $cnf_Profileing_URL;
global $cnf_GameLog_URL;
global $cnf_PROFILING_LOG_RATE;

$PF = Profiling::getInstance($cnf_Profileing_URL, $_SERVER['PHP_SELF'], $cnf_PROFILING_LOG_RATE);
$PF->startCheck('T_Page');

include "_LIB/lib_key.php";
include "_LIB/lib_DB.php";

$GameLog = GameLog::getInstance($cnf_GameLog_URL);

$BodyData = explode("\r\n", file_get_contents('php://input'));

//---------------------------------------------
// JSON 변환
//---------------------------------------------
$g_JSON = json_decode($BodyData[0], true);





?>
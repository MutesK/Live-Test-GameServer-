<?php

include "_LIB/lib_Profiling.php";
include "_LIB/lib_Log.php";

global $cnf_Profileing_URL;
global $cnf_GameLog_URL;
global $cnf_PROFILING_LOG_RATE;

$PF = Profiling::getInstance($cnf_Profileing_URL, $_SERVER['PHP_SELF'], $cnf_PROFILING_LOG_RATE);
$PF->startCheck('T_Page');

$GameLog = GameLog::getInstance($cnf_GameLog_URL);
$GameLog->AddLog(1, 0, 0);
$GameLog->SaveLog();


$PF->stopCheck('T_Page');
$PF->saveLog($g_AccountNo);

LOG_System(1, "Test", "None", 5);

?>
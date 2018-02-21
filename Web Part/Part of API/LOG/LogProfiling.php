<?php

include "_LIB/lib_DB.php";
echo "include Step";

global $g_DB;
DBConnect();
echo "DB Connect";

$Query = "INSERT INTO `profilinglog_".date("Ym")."` (date, IP, AccountNo, Action, T_Page, T_Mysql_Conn, T_Mysql, T_ExtAPI, T_LOG, Query, Comment)  
VALUES (NOW(), \"{$_POST['IP']}\", {$_POST['MemberNo']}, \"{$_POST['Action']}\", {$_POST['T_Page']},{$_POST['T_Mysql_Conn']},{$_POST['T_Mysql']},
{$_POST['T_ExtAPI']},{$_POST['T_Log']}, \"{$_POST['Query']}\", \"{$_POST['Comment']}\")";

echo $Query;

$DBResult = mysqli_query($g_DB,$Query);

if(!$DBResult)
{
    $errNo = mysqli_errno($g_DB);

    echo $errNo;

    if($errNo == 1146)
    {
        $TableQuery = "CREATE TABLE `profilinglog_".date("Ym")."` LIKE profilinglog_template";

        mysqli_query($g_DB, $TableQuery);
        mysqli_query($g_DB, $Query);
    }
}

DBDisconnect();

?>
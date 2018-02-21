<?php

include "_Startup.php";
include "_ResultCode.php";

$ResultCode = df_RESULT_SUCCESS;

if(!isset($g_JSON['email']) && !isset($g_JSON['accountno']))
    $ResultCode = df_PARAMETER_ERROR;

// 이메일 또는 계정번호를 이용해서 DBNO를 가져온다.
if($ResultCode != df_RESULT_SUCCESS)
{
    $ResJSON = array();
    $ResJSON['result'] = $ResultCode;

    ResponseJSON($ResJSON);
    exit;
}

$Rand = mt_rand(0, 1);
$IP = "";
switch($Rand)
{
    case 0:
        $IP = "P:172.16.1.1";
    break;
    case 1:
        $IP = "P:172.16.1.2";
    break;
}

$ShSlave_DB = new DBConnector($IP, "GameServer", "!ss640803", "shdb_index");

if(!$ShSlave_DB->DBConnect())
     $ResultCode = df_DB_INDEX_SLAVE_CONNECT_ERROR;


if($ResultCode != df_RESULT_SUCCESS)
{
    $ResJSON = array();
    $ResJSON['result'] = $ResultCode;

    ResponseJSON($ResJSON);
    exit;
}

if(isset($g_JSON['accountno']))
{
    $AccountNo = $g_JSON['accountno'];
    $Query = "SELECT dbno from allocate where accountno = $AccountNo";

    $DBResult = $ShSlave_DB->DB_ExecQuery($Query);
    $Result = mysqli_fetch_array($DBResult, MYSQLI_ASSOC);
    mysqli_free_result($DBResult);

    $DBNO = $Result['dbno'];

}
else if(isset($g_JSON['email']))
{
    $Email = $g_JSON['email'];
    $Query = "SELECT dbno, accountno from allocate where email = \"$Email\"";

    $DBResult = $ShSlave_DB->DB_ExecQuery($Query);
    $Result = mysqli_fetch_array($DBResult, MYSQLI_ASSOC);
    mysqli_free_result($DBResult);

    $DBNO = $Result['dbno'];
    $AccountNo = $Result['accountno'];
}

if($DBNO == 0)
    $ResultCode = df_NONE_REGISTER;

// DBNO를 통해 접속할 DB을 가져온다.
if($ResultCode != df_RESULT_SUCCESS)
{
    $ResJSON = array();
    $ResJSON['result'] = $ResultCode;

    ResponseJSON($ResJSON);
    exit;
}


$ShSlave_DB->DB_SwitchDBName("shdb_info");
$Query = "SELECT * from dbconnect where dbno = $DBNO";
$DBResult = $ShSlave_DB->DB_ExecQuery($Query);

$DBConnectArray = mysqli_fetch_array($DBResult, MYSQLI_ASSOC);
mysqli_free_result($DBResult);


// 해당 데이터 DB에 접속한다.
if($ResultCode != df_RESULT_SUCCESS)
{
    $ResJSON = array();
    $ResJSON['result'] = $ResultCode;

    ResponseJSON($ResJSON);
    exit;
}


$DataDB = new DBConnector('P:'.$DBConnectArray['ip'], $DBConnectArray['id'], 
$DBConnectArray['pass'], $DBConnectArray['dbname'], $DBConnectArray['port']);

if(!$DataDB->DBConnect())
     $ResultCode = df_DB_DATA_CONNECT_ERROR;

// 해당 회원의 모든 데이터를 JSON으로 얻어온다.
if($ResultCode != df_RESULT_SUCCESS)
{
    $ResJSON = array();
    $ResJSON['result'] = $ResultCode;
    exit;
}

$ResJSON = array();
$ResJSON['result'] = $ResultCode;

    $Query = "SELECT * FROM account where accountno = '$AccountNo'";

    $DBResult = $DataDB->DB_ExecQuery($Query);
    $count = mysqli_num_rows($DBResult); 
    $Result = mysqli_fetch_array($DBResult, MYSQLI_ASSOC);
    mysqli_free_result($DBResult);


    if($count == 0)
    {
        $ResultCode = df_DATA_ACCOUNT_NOT_EXIST;

        $ShMaster_DB = new DBConnector("P:172.16.2.1", "GameServer", "!ss640803", "shdb_index");
        
        if($ShMaster_DB->DBConnect())
        {
             $Query = "DELETE FROM allocate WHERE accountno = '$AccountNo'";
             $DBResult = $ShMaster_DB->DB_ExecQuery($Query);
        
             $ShMaster_DB->DB_SwitchDBName("shdb_info");
             $Query = "UPDATE available SET available = available + 1 WHERE dbno = $DBNO";
             $DBResult = $ShMaster_DB->DB_ExecQuery($Query); 
        }
    }
    else
    {
        foreach($Result as $Key => $Value)
        {
            $ResJSON[$Key] = $Value;
        }
    }


ResponseJSON($ResJSON);

$ShSlave_DB->DBDisconnect();
$DataDB->DBDisconnect();

include "_Cleanup.php";
?>
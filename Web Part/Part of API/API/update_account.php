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
    $Query = "SELECT dbno, accountno from  allocate where email = \"$Email\"";

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

$DataDB = new DBConnector($DBConnectArray['ip'], $DBConnectArray['id'], 
$DBConnectArray['pass'], $DBConnectArray['dbname'], $DBConnectArray['port']);

if(!$DataDB->DBConnect())
    $ResultCode = df_DB_DATA_CONNECT_ERROR;
    

//////////////////////////////////////////////////////////////////////////////////
// 예외사항 검사한다.

if($ResultCode != df_RESULT_SUCCESS)
{
    $ResJSON = array();
    $ResJSON['result'] = $ResultCode;
    
    ResponseJSON($ResJSON);
    exit;
}

    // index allocate 는 있으나 data.contetns 가 없음 
    // - 해결은 index allocate 를 삭제하여 초기화
    $Query = "SELECT COUNT(accountno) as Cnt FROM contents where accountno = '$AccountNo'";

    $DBResult = $DataDB->DB_ExecQuery($Query);
    $Result = mysqli_fetch_array($DBResult, MYSQLI_ASSOC);
    mysqli_free_result($DBResult);

    if($Result['Cnt'] == 0)
    {
        $ResultCode = df_DATA_CONTENTS_NOT_EXIST;

        $ShMaster_DB = new DBConnector("172.16.2.1", "GameServer", "!ss640803", "shdb_index");

        if($ShMaster_DB->DBConnect())
        {
            $Query = "DELETE FROM allocate WHERE accountno = '$AccountNo'";
            $DBResult = $ShMaster_DB->DB_ExecQuery($Query);

            $ShMaster_DB->DB_SwitchDBName("shdb_info");
            $Query = "UPDATE available SET available = available + 1 WHERE dbno = $DBNO";
            $DBResult = $ShMaster_DB->DB_ExecQuery($Query);
            $ShMaster_DB->DBDisconnect();
        }
    }

//////////////////////////////////////////////////////////////////////////////////

// JSON 키 값에 맞게 칼럼과 값을 UPDATE한다.
if($ResultCode != df_RESULT_SUCCESS)
{
    $ResJSON = array();
    $ResJSON['result'] = $ResultCode;
    
    ResponseJSON($ResJSON);
    exit;
}

$qryArr = array();

foreach($g_JSON as $Key => $Value)
{
    if($Key != "accountno" && $Key != "email")
    {
        array_push($qryArr, "UPDATE account SET `$Key`= \"$Value\" WHERE accountno = '$AccountNo'");
    }
}

if($DataDB->DB_TransectionQuery($qryArr) == -1)
{
    // 에러 처리
    $ResultCode = df_DB_QUERY_ERROR;
}

$ResJSON = array();
$ResJSON['result'] = $ResultCode;

ResponseJSON($ResJSON);

if(isset($ShSlave_DB))
    $ShSlave_DB->DBDisconnect();
if(isset($DBData))
    $DBData->DBDisconnect();

include "_Cleanup.php";
?>
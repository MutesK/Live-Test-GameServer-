<?php

 class DBConnector
 {
    private $DB_IP = '';
    private $DB_ID = '';
    private $DB_PASS = '';
    private $DB_NAME = '';
    private $DB_PORT = '';
    
    
    private $DBObject;

    function __construct($DB_IP, $DB_ID, $DB_PASS, $DB_NAME, $DB_PORT = 3306)
    {
        $this->DB_IP = $DB_IP;
        $this->DB_ID = $DB_ID;
        $this->DB_PASS = $DB_PASS;
        $this->DB_NAME = $DB_NAME;
        $this->DB_PORT = $DB_PORT;

    }

    function DBConnect()
    {
        global $g_AccountNo;
    
        if(!isset($g_AccountNo))
            $g_AccountNo = -1;


        $this->DBObject = mysqli_connect($this->DB_IP, $this->DB_ID, $this->DB_PASS, $this->DB_NAME,$this->DB_PORT);
 
        if(!$this->DBObject)
        {
            LOG_System($g_AccountNo, "MySQL Connect", "{$_SERVER['PHP_SELF']} -> lib_DB.php => ERROR# mysqli_connect() :". mysql_connect_error(), 3);
            QuitError("Server Error[DBC]");
            return false;
        }

        // 언어셋 맞춤
        mysqli_set_charset($this->DBObject, "utf8");
        return true;
    }

    function DBDisconnect()
    {
         if(isset($this->DBObject))
            mysqli_close($this->DBObject);
    }

    function DB_ExecQuery($Query)
    {
        global $g_AccountNo;

        if(!isset($g_AccountNo))
            $g_AccountNo = -1;

        $Result = mysqli_query($this->DBObject, $Query);

        if(!$Result)
        {
            LOG_System($g_AccountNo, "DB ExecQuery[". mysqli_errno($this->DBObject)." ]:", "{$_SERVER['PHP_SELF']} -> lib_DB.php => DB Query Error : ". mysqli_error($this->DBObject), 3);
            
            //QuitError("Server Error[DBQ]");
        }

        return $Result;
    }

    // 트랜젝션 쿼리
    function DB_TransectionQuery($qryArr)
    {
        global $g_AccountNo;

        if(!isset($g_AccountNo))
            $g_AccountNo = -1;

        $insert_idArr = array();

        mysqli_begin_transaction($this->DBObject);

        foreach($qryArr as $value)
        {
            if(is_string($value))
            {
               // LOG_System($g_AccountNo, "{$_SERVER['PHP_SELF']} > DB_TransectionQuery()", "QUERY:". $value);

                if(!mysqli_query($this->DBObject, $value))
                {
                    LOG_System($g_AccountNo, "{$_SERVER['PHP_SELF']} > DB_TransectionQuery()", "QUERY Error : ".$value ." /". mysqli_error($this->DBObject));
                    DB_TransectionFail();
                    return -1;
                }

                array_push($insert_idArr, mysqli_insert_id($this->DBObject));
            }
            else
            {
                LOG_System($g_AccountNo, "{$_SERVER['PHP_SELF']} > DB_TransectionQuery()", "QUERY Error : ".$value ." /".mysqli_error($this->DBObject));
                echo "DB Query Error :". mysqli_error($this->DBObject);
                
                DB_TransectionFail();
                exit;
            }
        }

        mysqli_commit($this->DBObject);
        return $insert_idArr;
    }

    function DB_TransectionFail()
    {
        mysqli_rollback($this->DBObject);
    }

    function DB_SwitchDBName($DB_NAME)
    {
        if(!mysqli_select_db($this->DBObject, $DB_NAME))
            return false;

        $this->DB_NAME = $DB_NAME;
    }

    function GetDBObject()
    {
        return $this->DBObject;
    }

}
?>
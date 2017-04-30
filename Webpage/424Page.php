
<?php
//DB Connect
$connection = mysqli_connect('mysql1.cs.clemson.edu','khattle','420noscopeblazeit');
mysqli_select_db($connection, 'ssadatabase');
$salt = 'b0t@t0';

$query = "SELECT *,
AES_DECRYPT('IP', '".$salt."') AS IP,
AES_DECRYPT('Time', '".$salt."') AS Time,
AES_DECRYPT('RTTMin', '".$salt."') AS RTTMin,
AES_DECRYPT('RTTMax', '".$salt."') AS RTTMax,
AES_DECRYPT('RTTAverage', '".$salt."') AS RTTAverage,
AES_DECRYPT('ID', '".$salt."') AS ID
FROM Iterations";
$result = mysqli_query($connection, $query);


// Printing results in HTML
echo "<h1>Database Contents</h1>";
echo "<table>\n";
echo "\t<tr>\n \t\t<td>IP</td>\t\t<td>Time</td>\t\t<td>RTTMin</td>\t\t<td>RTTMax</td>\t\t<td>RTTAverage</td>\t\t<td>ID</td>\n";

while ($line = mysqli_fetch_array($result, MYSQLI_ASSOC)) {
   echo "\t<tr>\n";
	echo "\t\t<td>" . $line['IP'] . "</td>\n";
   echo "\t\t<td>" . $line['Time'] . "</td>\n";
	echo "\t\t<td>" . $line['RTTMin'] . "</td>\n";
	echo "\t\t<td>" . $line['RTTMax'] . "</td>\n";
	echo "\t\t<td>" . $line['RTTAvg'] . "</td>\n";
	echo "\t\t<td>" . $line['ID'] . "</td>\n";
   echo "\t</tr>\n";
}

echo "</table>\n";

mysqli_free_result($result);

mysqli_close($connection);
?>

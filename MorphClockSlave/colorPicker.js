<!DOCTYPE html>
<html>
<head>
<meta charset=\"utf-8\">
<meta http-equiv=\"X-UA-Compatible\" content=\"IE=edge\">
<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">

<link rel=\"stylesheet\" href=\"https://maxcdn.bootstrapcdn.com/bootstrap/3.3.6/css/bootstrap.min.css\">
<script src=\"https://maxcdn.bootstrapcdn.com/bootstrap/3.3.6/js/bootstrap.min.js\"></script>
<script src=\"https://cdnjs.cloudflare.com/ajax/libs/jscolor/2.0.4/jscolor.min.js\"></script>
</head>
<body>
<div class=\"container\">
<div class=\"row\">
<h1>ESP Color Picker</h1>
<a type=\"submit\" id=\"change_color\" type=\"button\" class=\"btn btn-primary\">Change Color</a> 
<input class=\"jscolor {onFineChange:'update(this)'}\" id=\"rgb\">
</div>
</div>
<script>
function update(picker) {
  document.getElementById('rgb').innerHTML = Math.round(picker.rgb[0]) + ', ' +  Math.round(picker.rgb[1]) + ', ' + Math.round(picker.rgb[2]);
  document.getElementById(\"change_color\").href=\"?r=\" + Math.round(picker.rgb[0]*4.0117) + \"&g=\" +  Math.round(picker.rgb[1]*4.0117) + \"&b=\" + Math.round(picker.rgb[2]*4.0117);
}
</script>
</body>
</html>

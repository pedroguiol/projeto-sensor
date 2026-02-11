const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html lang="pt-br">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Sistema de Biometria</title>
</head>
<body>

  <h2>Cadastro de Aluno</h2>
  
  <form action="/cadastrar" method="POST" onsubmit="mostrarLoading()">
    
    <label for="nome">Nome:</label><br>
    <input type="text" id="nome" name="nome" placeholder="Digite seu nome"><br><br>

    <label for="matricula">Matrícula:</label><br>
    <input type="number" id="matricula" name="matricula" placeholder="Digite sua matrícula"><br><br>

    <input type="submit" value="INICIAR CADASTRO">
    
  </form> 

  <script>
    function mostrarLoading() {
      console.log("Enviando...");
    }
  </script>

</body>
</html>
)rawliteral";
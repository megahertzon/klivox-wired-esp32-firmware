# Contrato de aprovisionamiento BLE — Klivox Wired

Fecha de investigación: 2026-11-06, America/Bogota. Revisión local: `af5ec814388762b123db0a8fcac88b7ffc4cda53`.

Investigación estática del firmware y del SDK instalado en `C:/esp/v5.5.6/esp-idf`, contrastada con fuentes públicas. El usuario reporta un aprovisionamiento exitoso con la app oficial de Espressif; no se reprodujo aquí ni se capturó tráfico BLE. No se ejecutaron compilación, flash, borrado, cambios de configuración ni instalación de dependencias. Este documento es el único archivo creado.

Los resultados se distinguen como **verificado en código/configuración**, **documentado por el proveedor**, **inferencia** o **pendiente de prueba**. No se incluyen contraseñas, PoP, salt ni verifier reales. Los números de línea corresponden a esta revisión; pueden cambiar en revisiones posteriores.

## 1. Resultado ejecutivo

El transporte activo es **BLE GATT de `wifi_prov_scheme_ble`**, con **Security 2, patch 1: SRP6a y AES-GCM**. No es BluFi, Bluetooth Classic, un UART BLE ni una característica que acepte JSON con las credenciales Wi-Fi.

La sesión exige usuario y contraseña de aprovisionamiento. El firmware usa un conjunto fijo de desarrollo con salt/verifier compilados. La app necesita los valores autorizados correspondientes, obtenidos por un canal controlado; no debe deducirlos del nombre BLE ni incluirlos en documentación o logs.

Recomendación: un adaptador Flutter propio mediante MethodChannel —o Pigeon para tipar la interfaz— que utilice los SDK oficiales Android/iOS. Su soporte de Security 2 está documentado; la interoperabilidad exacta debe probarse con el firmware actual y versiones de SDK fijadas. No se recomienda implementar SRP/AES-GCM desde cero ni bajar a Security 1 para acomodar un plugin.

## 2. Evidencias del firmware

| Ref. | Archivo y líneas | Hallazgo respaldado |
| --- | --- | --- |
| F1 | [wifi_manager.c:40](C:/Users/nelso/klivox-wired/components/wifi_manager/wifi_manager.c:40), líneas 40–104 | Material fijo de desarrollo, `get_sec2_salt()` y `get_sec2_verifier()`; ramas de producción sin implementar |
| F2 | [wifi_manager.c:173](C:/Users/nelso/klivox-wired/components/wifi_manager/wifi_manager.c:173), 173–244 | Publicación best effort, conexión y backoff |
| F3 | [wifi_manager.c:246](C:/Users/nelso/klivox-wired/components/wifi_manager/wifi_manager.c:246), 246–382 | Eventos Wi-Fi, IP, provisioning, transporte BLE y sesión segura |
| F4 | [wifi_manager.c:385](C:/Users/nelso/klivox-wired/components/wifi_manager/wifi_manager.c:385), 385–413 | Nombre anunciado y respuesta de `custom-data` |
| F5 | [wifi_manager.c:445](C:/Users/nelso/klivox-wired/components/wifi_manager/wifi_manager.c:445), 445–555 | `start_provisioning()`: Security 2, UUID, endpoints, parámetros y logs |
| F6 | [wifi_manager.c:559](C:/Users/nelso/klivox-wired/components/wifi_manager/wifi_manager.c:559), 559–595 | `init_provisioning_manager()`, cinco intentos y memoria BLE reutilizable |
| F7 | [wifi_manager.c:635](C:/Users/nelso/klivox-wired/components/wifi_manager/wifi_manager.c:635), 635–747 | Inicialización NVS, almacenamiento FLASH, detección de configuración guardada y arranque |
| F8 | [wifi_manager.c:752](C:/Users/nelso/klivox-wired/components/wifi_manager/wifi_manager.c:752), 752–870 | Reset exclusivo de credenciales, worker, deinit y consultas de estado |
| F9 | [wifi_manager.h:13](C:/Users/nelso/klivox-wired/components/wifi_manager/include/wifi_manager.h:13), 13–83 | Eventos y APIs públicas; alcance de los snapshots |
| F10 | [app_setup.c:10](C:/Users/nelso/klivox-wired/main/app_setup.c:10), 10–34; [setup_button_manager.h:7](C:/Users/nelso/klivox-wired/components/setup_button_manager/include/setup_button_manager.h:7), 7–16 | SETUP GPIO33, activo LOW, 5000 ms y debounce de 50 ms; solicita reset Wi-Fi |
| F11 | [main.c:39](C:/Users/nelso/klivox-wired/main/main.c:39), 39–48; [main/CMakeLists.txt:1](C:/Users/nelso/klivox-wired/main/CMakeLists.txt:1) | Ruta activa de inicialización; las copias `.refactor-backup/` no son el programa compilado |
| F12 | [Kconfig:3](C:/Users/nelso/klivox-wired/components/wifi_manager/Kconfig:3), 3–119 | Opciones de transporte, seguridad, reintentos y QR; opciones disponibles no equivalen a selección efectiva |

### Configuración efectiva

Se inspeccionaron `sdkconfig` y `build/config/sdkconfig.h`, no solo `sdkconfig.defaults`.

| Parámetro | Valor encontrado | Evidencia |
| --- | --- | --- |
| Target | `esp32` | [sdkconfig:253](C:/Users/nelso/klivox-wired/sdkconfig:253) |
| Bluetooth | Habilitado, host Bluedroid, controlador BLE only; Classic y NimBLE deshabilitados | [sdkconfig:488](C:/Users/nelso/klivox-wired/sdkconfig:488), 488–542 y 783–785 |
| Transporte provisioning | BLE; SoftAP deshabilitado | [sdkconfig:2443](C:/Users/nelso/klivox-wired/sdkconfig:2443), 2443–2444 |
| Seguridad seleccionada | Security 2 Development; Security 1 y modo Production no seleccionados | [sdkconfig:2445](C:/Users/nelso/klivox-wired/sdkconfig:2445), 2445–2448; F5 |
| Soporte protocomm compilado | 0, 1 y 2; soporte de patch habilitado | [sdkconfig:2186](C:/Users/nelso/klivox-wired/sdkconfig:2186), 2186–2189 |
| BLE bonding / cifrado de enlace obligatorio | Ambos deshabilitados en provisioning | [sdkconfig:2388](C:/Users/nelso/klivox-wired/sdkconfig:2388), 2388–2389 |
| Respuestas mediante notificaciones BLE | Deshabilitadas; usar transporte de petición/escritura y lectura del SDK | [sdkconfig:2390](C:/Users/nelso/klivox-wired/sdkconfig:2390); S5 |
| Mantener BLE después del provisioning | Deshabilitado | [sdkconfig:2391](C:/Users/nelso/klivox-wired/sdkconfig:2391) |
| Autostop tras éxito | 30 s; puede terminar antes al consultar éxito | [sdkconfig:2387](C:/Users/nelso/klivox-wired/sdkconfig:2387); S1, S2 |
| Escaneo Wi-Fi | Todos los canales; máximo configurado de 16 entradas | [sdkconfig:2386](C:/Users/nelso/klivox-wired/sdkconfig:2386), 2386–2393 |
| Reintentos iniciales | Configurados 5; permitir corregir credenciales fallidas | [sdkconfig:2451](C:/Users/nelso/klivox-wired/sdkconfig:2451), 2451–2452 |
| Backoff normal | Inicial 1000 ms, máximo 60000 ms | [sdkconfig:2453](C:/Users/nelso/klivox-wired/sdkconfig:2453), 2453–2454 |
| QR | Opción habilitada, pero sin generación en la ruta activa | [sdkconfig:2455](C:/Users/nelso/klivox-wired/sdkconfig:2455); F5 |

Que protocomm incluya soporte para 0/1/2 **no significa** que el servicio acepte las tres variantes. `start_provisioning()` entrega `WIFI_PROV_SECURITY_2` al manager. No se encontró negociación propia que autorice un downgrade. El cliente debe comprobar la versión y rechazar incompatibilidades.

## 3. Parámetros BLE y de seguridad para Flutter

| Campo | Contrato actual |
| --- | --- |
| Nombre anunciado | `PROV_` seguido de seis dígitos hexadecimales mayúsculos: últimos tres bytes de la MAC Station. Formato `PROV_<MAC_SUFFIX>`, no un nombre único fijo. F4 |
| UUID primario | `021a9004-0382-4aea-bff4-6b3f1c5adfb4`. Derivado del arreglo real de 16 bytes little endian de F5:521–531, invirtiendo el orden para la representación UUID habitual |
| `service_key` | `NULL`. El esquema BLE lo ignora; no es una contraseña de pairing ni la contraseña de Wi-Fi. F5:503–509 |
| `security` / `sec_ver` | `2` |
| `sec_patch_ver` | `1`, verificado en el objeto `protocomm_security2` del SDK local, no en comentarios del ejemplo. S3 |
| Usuario SRP | Definido por `EXAMPLE_PROV_SEC2_USERNAME`. Valor omitido intencionalmente. Debe corresponder al material salt/verifier del dispositivo |
| Contraseña de aprovisionamiento / PoP | Definida por `EXAMPLE_PROV_SEC2_PWD`, distinta de la contraseña del router. Valor omitido. En las APIs móviles suele llamarse `proofOfPossession` |
| Parámetros del servidor | `wifi_prov_security2_params_t` estático con salt y verifier obtenidos por `get_sec2_salt()` / `get_sec2_verifier()`. No enviarlos desde Flutter como si fueran el PoP |
| Pairing del sistema | No se exige bonding ni cifrado BLE de enlace. La confidencialidad/autenticación relevante se establece en la sesión protocomm |
| Identidad | Nombre y UUID sirven para descubrimiento, no prueban pertenencia ni autenticidad del producto |

**PoP en Security 2:** el campo `pop` representa la contraseña SRP del cliente. El servidor recibe salt/verifier, no ese puntero de texto como parámetros de sesión. En desarrollo, el texto también está compilado en el firmware; no proviene de NVS, MAC, SETUP ni de un generador por unidad. Las ramas Production de ambos getters devuelven `ESP_ERR_NOT_SUPPORTED`; cambiar el selector a Production por sí solo impediría iniciar el alta. F1 y F5.

**QR pendiente:** aunque el Kconfig conserva `EXAMPLE_PROV_SHOW_QR=y` y existe la dependencia QR, la función activa termina con `(void)username`, `(void)pop` y el nombre del servicio en log. No llama a un generador QR ni publica un payload de alta. Los comentarios heredados sobre QR no establecen un formato implementado. Definir fuera de este trabajo el origen autorizado del usuario/PoP y el formato de etiqueta o alta manual.

### Endpoints

El SDK registra cinco endpoints estándar y Klivox añade uno. Las características usan UUID de 128 bits derivados del UUID primario; los identificadores de 16 bits de la tabla son campos insertados en esa base, **no servicios Bluetooth estándar independientes**. No fijar handles GATT. Descubrir nombres leyendo el descriptor Characteristic User Description **0x2901**. S1 y S5.

| Nombre | Campo UUID del SDK | Uso |
| --- | --- | --- |
| `proto-ver` | `0xFF53` | Información de protocolo/capacidades y seguridad; consultar antes del handshake |
| `prov-session` | `0xFF51` | Handshake SRP de Security 2 |
| `prov-scan` | `0xFF50` | Escaneo de APs realizado por el ESP32 |
| `prov-config` | `0xFF52` | SetConfig, ApplyConfig y GetStatus mediante Protocol Buffers |
| `prov-ctrl` | `0xFF4F` | Control estándar del estado del manager, incluido reset de fallo/reprovisioning según SDK; no equivale al botón SETUP de Klivox |
| `custom-data` | `0xFF54`, derivado de la asignación incremental y del único endpoint añadido; pendiente capturarlo en GATT | Respuesta fija de diagnóstico; no recibe credenciales Wi-Fi ni registra propiedad |

Para `custom-data`, `custom_prov_data_handler()` ignora el contenido y responde `SUCCESS` con byte NUL final. No valida cuenta, usuario, habitación, tenant ni éxito de Wi-Fi. Es opcional para el flujo de la app y no debe usarse como prueba de alta. F4:401–413, F5:539–549.

`proto-ver` proporciona `prov.ver`, `prov.sec_ver`, `prov.sec_patch_ver` y capacidades como `wifi_scan`. Leer valores reales; no confundir `prov.ver` con la versión del firmware Klivox. No se encontró una llamada a `wifi_prov_mgr_set_app_info()` para anunciar metadatos comerciales/versionado propio. S1:250–284.

## 4. Flujo Flutter ↔ ESP32

1. Verificar BLE y permisos en el teléfono. Si el equipo está provisionado, explicar el uso físico de SETUP; no esperar que aparezca siempre en el escaneo.
2. Descubrir por UUID y prefijo `PROV_`, mostrar candidatos y dejar elegir la unidad. Conservar el identificador opaco de la plataforma; no exigir una MAC BLE visible en iOS ni equipararla a la MAC Station.
3. Conectar GATT, descubrir endpoints por descriptor y consultar `proto-ver` con el SDK. Verificar Security 2 y patch compatible. No enviar credenciales mediante una conexión GATT sin sesión segura.
4. Proporcionar usuario SRP y contraseña de aprovisionamiento autorizados al SDK. Completar `prov-session`; un BLE conectado no implica sesión autenticada.
5. Opcionalmente solicitar escaneo Wi-Fi en `prov-scan`. Es la visibilidad del ESP32, no la lista Wi-Fi del teléfono. El ESP32 clásico requiere un AP compatible de 2,4 GHz; no prometer Wi-Fi de 5 GHz ni Enterprise con solo SSID/contraseña.
6. Obtener SSID y contraseña del router. Enviar `CmdSetConfig` y comprobar su respuesta. El SDK guarda temporalmente la configuración de esa petición; eso aún no prueba conexión.
7. Enviar `CmdApplyConfig`. El manager selecciona almacenamiento FLASH y llama `esp_wifi_set_config()`; inicia conexión después de aproximadamente 1 s para permitir responder antes del cambio de canal. Las credenciales quedan en NVS **antes** de obtener IP. S1:1280–1343; S2:90–144.
8. Consultar `CmdGetStatus`, serializando peticiones de la sesión. Mantener diferenciados configuración aplicada, conexión pendiente, intento fallido con intentos restantes y resultado terminal.
9. Confirmar éxito únicamente al recibir un estado `Connected` con la información de conexión IPv4, o el callback final equivalente del SDK. No usar como éxito el ACK de ApplyConfig, el endpoint personalizado, la conexión BLE ni su desconexión.
10. Tras registrar el resultado, liberar recursos del cliente. El SDK del dispositivo llama `wifi_prov_mgr_done()` al servir un GetStatus conectado y prepara la detención; si nadie consulta, el autostop de 30 s tras IP ofrece otra ruta. No programar operaciones esenciales después de esa confirmación. S1, S2.

Usar Protocol Buffers y transporte de los SDK oficiales, incluyendo fragmentación/MTU y lectura de respuestas. El firmware no exige un MTU particular al cliente; no inventar un tamaño fijo ni implementar una escritura JSON equivalente. Mantener una operación lógica en curso por sesión; tras desconexión crear otra sesión, sin reutilizar claves/contadores AES-GCM.

Límites observados en los handlers: SSID de hasta 32 bytes y passphrase copiada con terminación en un buffer de 64 bytes, permitiendo 63 bytes de texto. Validar bytes/codificación en la app y probar SSID no ASCII. No prometer una PSK hexadecimal de 64 caracteres con esta ruta. S2:105–117.

### Contrato propuesto del adaptador Flutter

Esto es diseño de integración, **no una API ya implementada**. El firmware no se modifica.

| Operación lógica | Datos / resultado |
| --- | --- |
| `discover` | Prefijo y UUID anteriores → identificador opaco, nombre y RSSI; sin secretos |
| `connectAndAuthenticate` | Identificador, usuario, contraseña de aprovisionamiento → sesión autenticada y versiones observadas |
| `scanWifi` | Sesión → SSID, RSSI y modo de seguridad según soporte real del SDK |
| `provision` | Sesión, SSID y passphrase → progreso y resultado final con IPv4 o error clasificado |
| `cancel` / `disconnect` | Cancelar trabajo local, resolver futures una sola vez y liberar BLE. No garantiza revertir ApplyConfig ni borrar NVS |

Estados recomendados: `discovering`, `connectingBle`, `authenticating`, `sessionReady`, `scanningWifi`, `configuring`, `waitingForIp`, `succeeded`, `failed`, `cancelled` y `outcomeUnknown`. Enviar progreso por EventChannel o mecanismo tipado; reservar MethodChannel para comandos/resultados. Distinguir el ID de operación para descartar callbacks tardíos. Nunca registrar argumentos que contengan credenciales.

El aprovisionamiento Wi-Fi no registra propiedad en el backend ni confirma Internet/MQTT. El firmware actual no incorpora ese contrato; la app no debe presentar estas garantías como parte del éxito BLE.

## 5. Estados y eventos del dispositivo

Los siguientes eventos son internos de ESP-IDF/FreeRTOS. **No son notificaciones GATT automáticamente visibles desde Flutter.** El teléfono observa las respuestas de protocolo o callbacks del SDK móvil.

| Evento | Qué confirma / qué no confirma |
| --- | --- |
| `PROTOCOMM_TRANSPORT_BLE_CONNECTED` | Enlace BLE; todavía no autentica |
| `PROTOCOMM_SECURITY_SESSION_SETUP_OK` | Handshake de sesión completado; todavía no conecta Wi-Fi |
| `WIFI_PROV_START` → `WIFI_MANAGER_EVENT_PROVISIONING_STARTED` | Servicio de alta iniciado |
| `WIFI_PROV_CRED_RECV` | Credenciales aplicadas al manager; el firmware no imprime su contenido |
| `WIFI_EVENT_STA_CONNECTED` | Asociación con AP; puede faltar DHCP |
| `IP_EVENT_STA_GOT_IP` → `WIFI_MANAGER_EVENT_CONNECTED` | IPv4 lista; payload público con IP |
| `WIFI_PROV_CRED_SUCCESS` → `WIFI_MANAGER_EVENT_PROVISIONING_SUCCESS` | El manager alcanzó éxito con IP; Klivox marca `PROVISIONED_BIT` |
| `WIFI_PROV_CRED_FAIL` → `WIFI_MANAGER_EVENT_PROVISIONING_FAILED` | Alta fallida; clasifica auth/AP o fallo interno según origen |
| `WIFI_EVENT_STA_DISCONNECTED` / IP perdida | Publica desconexión y aplica la política de reconexión que corresponda |
| `WIFI_PROV_END` | El worker recibe trabajo para `wifi_prov_mgr_deinit()` |
| `WIFI_MANAGER_EVENT_PROVISIONING_ENDED` | Recursos liberados y condición de provisioning retirada |

No depender del orden de entrega relativo de CONNECTED y PROVISIONING_SUCCESS. Las publicaciones públicas usan cola con espera cero y pueden descartarse si se satura; los consumidores locales tienen snapshots `is_connected()`, `is_provisioning()` e `is_provisioned()`. No hay endpoint BLE propio para consultar esos flags de aplicación. F2, F3, F8 y F9.

## 6. Ciclo de vida

| Escenario | Comportamiento verificado / implicación para Flutter |
| --- | --- |
| 1. Primer encendido sin SSID guardado | Inicializa NVS sin borrarla, Wi-Fi/netif/event loop y manager. `is_provisioned()` devuelve falso; inicia BLE y espera credenciales. F7 |
| 2. Reinicio con credenciales | `wifi_prov_mgr_is_provisioned()` comprueba SSID no vacío. Libera el manager e inicia Station sin BLE. No verifica primero que ese router siga disponible. F7, S1:1246–1264 |
| 3. Pérdida de Wi-Fi | Conserva credenciales. Fuera del provisioning, reconecta con backoff 1, 2, 4… hasta 60 s. No habilita BLE automáticamente por perder el AP. Si solo se pierde IP y continúa asociado, espera recuperación DHCP. F2, F3 |
| 4. Cambio de router/contraseña | Reintenta las credenciales antiguas, sin borrarlas por fallos. Para configurar otro router, el usuario mantiene SETUP y vuelve a aprovisionar. No hay comando comercial remoto de cambio ya implementado. F3, F8 |
| 5. SETUP | GPIO33 LOW durante 5 s solicita reset asíncrono. El worker deinitializa manager, detiene Station, drena eventos antiguos, selecciona FLASH y guarda una configuración Station vacía; recrea manager y BLE sin reiniciar el ESP32. No borra toda NVS ni usa `esp_wifi_restore()`. Dos pitidos confirman pulsación, no alta exitosa. F8, F10 |
| 6. BLE se desconecta durante el alta | La app Klivox solo registra el evento. El transporte cierra la sesión segura, limpia respuestas anteriores y vuelve a anunciar mientras siga activo. No revierte credenciales ya aplicadas ni detiene por sí mismo la conexión Wi-Fi. Si aún hay BLE, reconectar y hacer handshake nuevo; si el resultado se perdió, informar resultado desconocido. S5, F3 |
| 7. Éxito y posterior reinicio | El SDK obtuvo IP, las credenciales persistieron y BLE se detuvo por finalización/autostop. Tras reiniciar, el caso 2 conecta Station sin BLE. No hay reinicio automático al aprovisionar correctamente. F3, F7, F8 |

### Cuándo inicia y termina BLE

Inicia únicamente al arrancar sin credenciales o al completar la ruta de reset explícita. No se encontró un timeout de inactividad propio para cerrar una ventana BLE que nunca obtiene éxito.

Termina al detenerse el manager tras éxito, al pedir stop/deinit, durante reset o ante limpieza por error de arranque. El worker hace la deinitialización fuera del event loop. Se usa `WIFI_PROV_EVENT_HANDLER_NONE`, por lo que no se ejecuta el manejador que libera memoria BTDM irreversiblemente: el transporte se detiene, pero puede recrearse para otro SETUP en el mismo boot. F6, F8. No confundir esta política con `FREE_BTDM` del ejemplo genérico de Espressif.

**Caso límite de persistencia:** se escribe FLASH antes de comprobar IP y el criterio de provisionado en el siguiente boot es SSID no vacío. Por inferencia, cortar alimentación durante una validación fallida, antes de que se descarten las credenciales nuevas, puede dejar un SSID inválido guardado y arrancar sin BLE. Documentar SETUP como recuperación; no afirmar que solo se persisten credenciales ya validadas.

## 7. Manejo de errores

| Error / situación | Comportamiento y respuesta de la app |
| --- | --- |
| PoP/usuario incorrectos, fallo SRP o GCM | No enviar credenciales. Mostrar fallo de autenticación de provisioning, distinto de contraseña Wi-Fi incorrecta. Reconectar con una sesión nueva si corresponde |
| Permiso denegado / Bluetooth apagado | Explicar acción necesaria en el teléfono, sin sugerir reset Wi-Fi |
| No se encuentra el dispositivo | Confirmar modo SETUP, distancia y selección; un equipo provisionado no anuncia normalmente |
| Configuración aceptada / aplicada | Mantener espera hasta estado conectado o fallo terminal; no marcar éxito anticipado |
| AP ausente / contraseña incorrecta | El SDK efectúa los intentos configurados. Klivox publica fallo y ejecuta `wifi_prov_mgr_reset_sm_state_on_failure()` solo si aún no fue validado; descarta nuevas credenciales rechazadas y deja repetir el envío sin borrar una instalación previamente validada |
| Intento intermedio fallido | Mostrar reintento/espera; no tratar `ConnectionFailed` con intentos restantes como éxito ni como desconexión BLE |
| Tiempo agotado sin respuesta final | Separar timeout de conexión BLE de validación Wi-Fi/DHCP. Cinco intentos no equivalen a cinco segundos. Los 30 s de autostop se cuentan tras éxito; no son timeout universal del alta |
| BLE perdido tras ApplyConfig | El resultado puede ser desconocido o el servicio haber finalizado con éxito. No borrar ni reprovisionar automáticamente; conservar un resultado final ya recibido |
| NVS/inicio/reset fallan | No se borra automáticamente toda NVS; se devuelve/loguea error o publica fallo interno. `ESP_OK` al solicitar SETUP significa petición aceptada, no finalización |

`prov-ctrl` existe mientras el manager está activo, pero sus operaciones estándar llaman funciones del SDK y no reproducen necesariamente la política completa de flags/reset del wrapper Klivox. No usarlas para cambiar el router de una instalación como si fueran `wifi_manager_reset_credentials()`. Después de cerrar BLE no constituyen una API remota disponible. S1, S2, F8.

**Hallazgos de lectura del SDK que requieren prueba dirigida, sin corregirlos aquí:**

- En `manager.c:990–1017`, se copia el motivo anterior de desconexión antes de actualizar el campo con la razón actual, y se emite el evento con esa copia. El motivo publicado por el wrapper puede diferir del obtenido por GetStatus. Es un indicio estático; reproducir con contraseña incorrecta y AP ausente antes de atribuir una causa definitiva en UI.
- El contador `connection_attempts_completed` aumenta en `manager.c:969`; `reset_sm_state_on_failure()` en 1776–1804 restablece `prov_state` pero no ese contador. No prometer cinco intentos nuevos para cada reenvío dentro de la misma sesión sin verificar ese caso. No se parcheó el SDK.

## 8. Seguridad real y riesgos de producción

**Sesión:** SRP6a permite probar conocimiento de la contraseña y derivar claves sin enviarla como texto; AES-GCM cifra y autentica mensajes. Patch 1 modifica el manejo de IV/contador, por lo que la etiqueta genérica “soporta Security 2” no basta para seleccionar cliente. Esto protege el intercambio una vez establecida correctamente la sesión; no cifra nombre/UUID/descriptores de descubrimiento ni demuestra propiedad en un backend. S3 y fuentes E1–E3.

**Autorización:** el material de desarrollo es fijo y compartido por las unidades que usen esta imagen. Quien lo conozca y tenga alcance BLE durante la ventana de alta puede intentar aprovisionar. SETUP restringe el acceso a una nueva ventana en equipos ya configurados, pero no vincula al primer teléfono, a una cuenta ni a una identidad de instalador. No se encontró un control de rate limit, expiración de ventana ni verificación de propiedad en `custom-data`. No presentar Security 2 como sustituto de esos controles.

**Almacenamiento:** Wi-Fi NVS está habilitado. Secure Boot y flash encryption no están seleccionados en [sdkconfig:357](C:/Users/nelso/klivox-wired/sdkconfig:357)–358; no aparece NVS encryption en la configuración generada, coherente con las dependencias del Kconfig NVS para ESP32. Esto no ofrece protección configurada frente a lectura física del contenido flash. No se leyó eFuse ni se auditó la placa: no se afirma su estado físico. La persistencia Wi-Fi no debe confundirse con cifrado en reposo.

**Logs:** el wrapper evita registrar credenciales y el contenido arbitrario de `custom-data`. Nivel actual INFO y máximo INFO: [sdkconfig:1751](C:/Users/nelso/klivox-wired/sdkconfig:1751)–1761. El SDK tiene logs de credenciales en DEBUG; no habilitarlos indiscriminadamente. Además, las fuentes móviles consultadas incluyen un log del PoP en Android y de argumentos de aprovisionamiento en iOS. Auditar la versión fijada y comprobar redacción/desactivación en release y crash analytics; no suponer que una biblioteca oficial nunca registra secretos. E4–E5.

### Recomendaciones para una futura fase, no implementadas

1. Definir fabricación de credenciales únicas de aprovisionamiento por unidad, entrega controlada al usuario —por ejemplo etiqueta QR— y almacenamiento de salt/verifier del servidor. Evitar una contraseña global embebida en la app o recuperable de todas las imágenes.
2. Mantener Security 2 patch compatible; fijar versiones de ambos SDK y rechazar downgrade. Validar identidad/propiedad comercial por un contrato separado si el producto lo requiere.
3. Diseñar ventana de provisioning limitada, política de reintentos y recuperación física; evaluar rate limiting sin bloquear permanentemente al propietario.
4. Evaluar Secure Boot, flash encryption y NVS encryption con un proceso de fabricación/recuperación antes de habilitarlos. No cambiar eFuses o credenciales en esta investigación.
5. Mantener contraseñas de router y provisioning fuera de logs/telemetría, con retención mínima en memoria; no persistirlas en preferencias de texto de la app. Si hubiera que conservar un secreto de alta, definir antes el uso de almacenamiento seguro nativo.
6. Separar éxito IPv4, acceso a Internet, autenticación MQTT y asociación a cuenta. Este firmware solo acredita el primero en el contrato actual.

## 9. Compatibilidad con Flutter y alternativas

Consulta de fuentes públicas al 2026-11-06. “Verificado” en esta sección significa inspección de documentación/código, **no una app Flutter ejecutada con el hardware**. Las ramas `master` y páginas `latest` no son versiones congeladas de distribución.

| Alternativa | Evidencia y licencia declarada | Evaluación para Klivox |
| --- | --- | --- |
| SDK oficial Android | README declara BLE, scan, provisioning y Security 2; `ESPDevice.java` procesa `sec_patch_ver` y crea Security2. Apache-2.0. E1, E4, E6 | Opción prioritaria mediante adaptador Kotlin/Java. Probar tag concreto, patch 1 y errores. README declara Android 8/API26+, aunque `build.gradle` consultado tiene minSdk23: resolver soporte efectivo al fijar versión |
| SDK oficial iOS `ESPProvision` | README declara Security 2; delegate obtiene PoP/usuario; código `.secure2` interpreta patch y contador. Apache-2.0. E2, E5, E7 | Opción prioritaria mediante Swift. README indica iOS13+; comprobar pod/SPM/tag concretos, patch 1 y comportamiento de cierre BLE |
| Flutter MethodChannel / Pigeon | Mecanismo oficial para invocar código nativo; no implementa el protocolo Espressif. E8 | Flutter gestiona UX; nativo encapsula criptografía/transporte. MethodChannel para comandos y EventChannel para progreso; Pigeon puede tipar el puente. Requiere mantenimiento de dos plataformas |
| `esp_provisioning_wifi` | Pub.dev consultado: 0.3.1, Android/iOS, MIT; documenta `security2` más `username`. E9 | Candidato para una prueba técnica, no aprobado. Verificar implementación nativa, patch 1, UUID configurable/descubrimiento, dependencias, logs y cancelación. La documentación de Security 2 por sí sola no verifica interoperabilidad |
| `esp_provisioning_ble` | Pub.dev consultado: 1.1.0, MIT; declara que Security 2 aún no está disponible. E10 | No compatible directamente con la configuración actual; añadir criptografía propia sería otro alcance, no justifica degradar firmware |
| `flutter_esp_ble_prov` | Declara puente a bibliotecas Espressif y licencia MIT; documentación examinada no acredita Security 2 patch 1 ni contrato de usuario. E11 | Pendiente de auditoría; no seleccionarlo por el nombre o por funcionar con Security 1 |

Las licencias anteriores identifican las declaraciones de los proveedores, no sustituyen revisar los archivos LICENSE/NOTICE y dependencias transitivas de la versión elegida. Mantener atribuciones y un inventario de licencias; no asumir que MIT en un wrapper cubre sus SDK nativos o su transporte BLE. No se añadió ningún paquete.

### Android e iOS

Android 12+ exige resolver permisos Bluetooth de escaneo/conexión en runtime según el target; Android anteriores pueden requerir ubicación para escaneo. `neverForLocation` tiene condiciones y puede filtrar resultados: probarlo con el anuncio real, no copiar un manifiesto genérico. Consultar E12. La lista de APs llega del ESP32 por BLE y no exige que el teléfono se una a la red objetivo.

En iOS, declarar el propósito Bluetooth (`NSBluetoothAlwaysUsageDescription`) y manejar estados de CoreBluetooth. El identificador de periférico de iOS no es la MAC Station. Revisar por separado los requisitos del SDK para SoftAP, lectura del SSID del teléfono o red local: no asumir que todas esas capacidades son necesarias para este flujo BLE. E2, E13. Mantener el proceso inicialmente en foreground y probar interrupciones/bloqueo de pantalla en teléfonos reales.

## 10. Pendientes antes de desarrollar la app de producto

- Obtener por vía autorizada el usuario y contraseña de desarrollo para el banco de pruebas, sin incorporarlos a este documento. Definir cómo se entregarán los de producción y si se permitirá entrada manual/QR.
- Elegir y fijar tags/commits de Android/iOS o del plugin, revisar licencias/dependencias y validar Security 2 patch 1 en ambos teléfonos con la misma imagen del ESP32.
- Capturar GATT real: nombre anunciado, UUID primario y descriptores de los seis endpoints. Confirmar que el adaptador no presupone otro UUID ni notificaciones.
- Definir versiones mínimas de Android/iOS, permisos, timeouts, comportamiento de cancelación y resultado desconocido; no usar una espera única de 30 s para todo.
- Probar éxito, PoP/usuario incorrectos, router ausente, passphrase incorrecta, SSID Unicode, pérdida BLE antes/después de ApplyConfig, pérdida BLE tras recibir éxito y reenvíos múltiples.
- Probar corte eléctrico durante validación, reinicio tras éxito, router cambiado y SETUP repetido sin reboot. Verificar persistencia y aislamiento de otras configuraciones NVS.
- Confirmar los dos hallazgos del SDK sobre razón de fallo y contador, sin ocultarlos mediante textos de error demasiado específicos.
- Definir identidad comercial y asociación a cuenta por separado: el UUID genérico y `custom-data` no cumplen esa función.
- Auditar logs release de firmware, bibliotecas nativas y Flutter. Documentar política de protección de secretos en fabricación, dispositivo y móvil.

## 11. Fuentes SDK locales y referencias externas

### SDK ESP-IDF 5.5.6 instalado, solo leído

| Ref. | Archivo y líneas relevantes |
| --- | --- |
| S1 | [wifi_provisioning/src/manager.c:250](C:/esp/v5.5.6/esp-idf/components/wifi_provisioning/src/manager.c:250): metadatos 250–284; endpoints 319–425 y 1406–1438; éxito/autostop/fallo 951–1017; criterio guardado 1246–1264; persistencia 1280–1343; reset 1776–1804; `wifi_prov_mgr_done()` desde 775; cleanup delay 1440–1442 |
| S2 | [wifi_provisioning/src/handlers.c:46](C:/esp/v5.5.6/esp-idf/components/wifi_provisioning/src/handlers.c:46): GetStatus e IP 46–87; límites y ApplyConfig 90–144; control 212–228. [wifi_config.proto:6](C:/esp/v5.5.6/esp-idf/components/wifi_provisioning/proto/wifi_config.proto:6) y [wifi_constants.proto:3](C:/esp/v5.5.6/esp-idf/components/wifi_provisioning/proto/wifi_constants.proto:3): mensajes, estados y motivos |
| S3 | [protocomm/src/security/security2.c:623](C:/esp/v5.5.6/esp-idf/components/protocomm/src/security/security2.c:623): `ver=2`, `patch_ver=1`, callbacks de cifrado y sesión |
| S4 | [wifi_provisioning/src/scheme_ble.c:53](C:/esp/v5.5.6/esp-idf/components/wifi_provisioning/src/scheme_ble.c:53): opciones BLE 53–74; nombre/UUID 158–176; manejadores opcionales de memoria 248–315. Klivox no selecciona esos manejadores FREE |
| S5 | [protocomm/src/transports/protocomm_ble.c:510](C:/esp/v5.5.6/esp-idf/components/protocomm/src/transports/protocomm_ble.c:510): cierre de sesión/respuestas 510–552; características y descriptor 646–708; UUID derivado 833–836. [simple_ble.c:203](C:/esp/v5.5.6/esp-idf/components/protocomm/src/simple_ble/simple_ble.c:203): reanudación de advertising tras desconexión |
| S6 | [nvs_flash/Kconfig:3](C:/esp/v5.5.6/esp-idf/components/nvs_flash/Kconfig:3): dependencias del cifrado NVS. Configuración generada: [sdkconfig.h:830](C:/Users/nelso/klivox-wired/build/config/sdkconfig.h:830), 830–833, 890–892 y 911–919 |

### Fuentes primarias consultadas en web

- E1. [SDK Android: README oficial](https://github.com/espressif/esp-idf-provisioning-android/blob/master/README.md). Soporte, usuario/PoP y flujo; [build.gradle](https://github.com/espressif/esp-idf-provisioning-android/blob/master/provisioning/build.gradle) para contraste de requisitos.
- E2. [SDK iOS: README oficial](https://github.com/espressif/esp-idf-provisioning-ios/blob/master/README.md). Security 2, delegates, flujo y requisitos publicados.
- E3. [Wi-Fi provisioning ESP-IDF 5.5.6](https://docs.espressif.com/projects/esp-idf/en/v5.5.6/esp32/api-reference/provisioning/wifi_provisioning.html) y [protocomm 5.5.6](https://docs.espressif.com/projects/esp-idf/en/v5.5.6/esp32/api-reference/provisioning/protocomm.html). Los detalles exactos de Klivox se obtuvieron del código local, no del ejemplo genérico.
- E4. [ESPDevice.java](https://github.com/espressif/esp-idf-provisioning-android/blob/master/provisioning/src/main/java/com/espressif/provisioning/ESPDevice.java). `initSession()`, manejo de patch y log del PoP; fuentes web observadas aproximadamente en líneas 644–706.
- E5. [ESPDevice.swift](https://github.com/espressif/esp-idf-provisioning-ios/blob/master/ESPProvision/ESPDevice.swift). `provision()` incluye log de argumentos cerca de 382–383; inicialización Security 2/patch cerca de 655–662.
- E6. [LICENSE Android](https://github.com/espressif/esp-idf-provisioning-android/blob/master/LICENSE).
- E7. [LICENSE iOS](https://github.com/espressif/esp-idf-provisioning-ios/blob/master/LICENSE).
- E8. [Flutter: platform channels](https://docs.flutter.dev/platform-integration/platform-channels). Puentes asíncronos y tipado mediante Pigeon.
- E9. [Documentación del autor de esp_provisioning_wifi](https://pub.dev/packages/esp_provisioning_wifi) y [changelog](https://pub.dev/packages/esp_provisioning_wifi/changelog).
- E10. [Documentación del autor de esp_provisioning_ble](https://pub.dev/packages/esp_provisioning_ble), sección Security: ausencia declarada de Security 2.
- E11. [Documentación del autor de flutter_esp_ble_prov](https://pub.dev/packages/flutter_esp_ble_prov).
- E12. [Android: permisos Bluetooth](https://developer.android.com/develop/connectivity/bluetooth/bt-permissions).
- E13. [Apple: descripción de uso Bluetooth](https://developer.apple.com/documentation/bundleresources/information-property-list/nsbluetoothalwaysusagedescription).

La evidencia externa permite seleccionar candidatos; no certifica compatibilidad, mantenimiento futuro ni seguridad de la aplicación que aún no existe. No se fijaron dependencias ni se realizaron cambios de firmware en esta investigación.

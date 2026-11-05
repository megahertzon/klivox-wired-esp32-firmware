# Klivox Wired

## Contexto

- Firmware comercial para automatizar aires acondicionados.
- Hardware: ESP32-D0WD-V3, revisión 3, flash de 4 MB.
- Desarrollo en Windows con VS Code, C, FreeRTOS y ESP-IDF 5.5.6.
- Consulta `README.md` para conocer el funcionamiento, la API y las pruebas
  manuales. Inspecciona el código y la configuración actuales antes de modificar.

## Arquitectura

- Mantén `main/main.c` enfocado en la inicialización general.
- Encapsula la lógica de cada módulo, sus eventos y sus recursos. La gestión
  Wi-Fi y del aprovisionamiento BLE pertenece a `wifi_manager`.
- Otros módulos deben consumir la API pública y `WIFI_MANAGER_EVENT`; evita
  duplicar la inicialización del controlador o los manejadores de conectividad.
- Mantén compatibilidad con FreeRTOS. Evita esperas prolongadas en el flujo
  principal, bucles de polling y operaciones bloqueantes en callbacks de eventos.
- Prefiere cambios incrementales y las APIs oficiales de ESP-IDF. Agrega
  dependencias externas solo cuando sean necesarias para la tarea.
- La integración prevista de MQTT usará TLS y los eventos de conectividad;
  desarrolla esa funcionalidad cuando forme parte del alcance solicitado.

## Comportamiento que debe preservarse

- Aprovisionamiento inicial mediante el Provisioning Manager oficial por BLE.
- Configuración actual de Security Version 2 en modo Development.
- Conexión automática con credenciales persistentes, sin iniciar BLE cuando el
  equipo ya está provisionado.
- Reconexión con espera progresiva, sin borrar credenciales por fallos temporales
  ni reiniciar innecesariamente el equipo.
- Persistencia Wi-Fi en NVS durante reinicios y cortes de alimentación.
- Liberación de los recursos del aprovisionamiento BLE al finalizar.
- Conserva la partición `Single factory app (large), no OTA`, salvo que la tarea
  solicite expresamente cambiarla.

## Seguridad y datos persistentes

- No registres contraseñas Wi-Fi ni datos arbitrarios del cliente que puedan
  contener secretos. No introduzcas secretos reales en código o documentación.
- Conserva las credenciales, salt, verifier y UUID de aprovisionamiento actuales
  durante cambios ajenos a ellos. Avisa y justifica cualquier cambio necesario.
- Mantén separadas las rutas de desarrollo y producción. El material compartido
  de desarrollo no debe utilizarse como configuración de producción.
- No borres NVS automáticamente ante errores de inicialización ni desconexiones.
  El reintento de aprovisionamiento inicial solo puede descartar las credenciales
  nuevas rechazadas; debe conservar las de una instalación ya validada.
- No ejecutes flash, borrado de memoria ni operaciones destructivas sobre el
  dispositivo sin autorización explícita vigente del usuario.

## Cambios y validación

- No modifiques archivos del SDK de Espressif ni edites a mano los archivos
  generados de `build/` o los paquetes de `managed_components/`.
- Actualiza CMake y las dependencias declaradas cuando cambien los módulos.
- Tras modificar C, CMake, Kconfig o dependencias, ejecuta `idf.py build` con
  ESP-IDF 5.5.6 y corrige los errores antes de entregar. Un cambio exclusivamente
  documental no requiere recompilar.
- En este entorno Windows, si el sandbox bloquea los sockets internos de Python
  utilizados por ESP-IDF, ejecuta la compilación fuera del sandbox mediante el
  mecanismo de permisos disponible.
- Distingue entre compilación verificada y funcionamiento comprobado en hardware.
  Nunca presentes una compilación exitosa como prueba de BLE, radio o NVS.
- Si cambia la conectividad, indica cómo verificar aprovisionamiento inicial,
  persistencia tras cortar alimentación y recuperación después de perder el AP.
- Actualiza `README.md` cuando cambie el comportamiento o la API pública.
  Mantén este archivo breve y centrado en reglas duraderas, sin historial de tareas.
- Comunica en español los cambios, la validación realizada y lo pendiente.

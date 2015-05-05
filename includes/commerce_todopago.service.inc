<?php

/**
 * @file
 * Wrapper class for the Todo Pago gateway.
 */

/**
 * Todo Pago connector using the drupal stored credentials.
 */
class TodoPagoConnector {
  private $settings;
  private $todopago;

  /**
   * Constructor
   */
  public function __construct($settings) {
    if (
      (empty($settings)) ||
      (!is_array($settings)) ||
      (empty($settings['server'])) ||
      (empty($settings['merchant'])) ||
      (empty($settings['authorization'])) ||
      (empty($settings['security'])) ||
      (empty($settings['currency_code']))
    ) {
      throw new Exception(t('Todo Pago is not configured for use.'));
    }
    $this->settings = $settings;

    $http_header = array(
      'Authorization' => $this->settings['authorization'],
      'user_agent' => 'Commerce_TodoPago',
    );
    $wsdl = $this->get_wsdl($this->settings['server']);
    $end_point = $this->get_end_point($this->settings['server']);

    $this->todopago = new TodoPago($http_header, $wsdl, $end_point);
  }

  /**
   * Returns the URL to the specified Todo Pago's API server.
   *
   * @param $server
   *   Either sandbox or live indicating which server to get the URL for.
   *
   * @return
   *   The URL to use to submit requests to the Todo Pago's API server.
   */
  private function get_end_point($server) {
    switch ($server) {
      case 'sandbox':
        return 'https://50.19.97.101:8243/services/';
      case 'live':
        return 'https://50.19.97.101:8243/services/';
    }
  }

  /**
   * Returns the Todo Pago's WSDLs.
   *
   * @return
   *   An array with the Todo Pago's WSDLs.
   */
  private function get_wsdl($server) {
    return array(
      'Authorize' => $this->get_end_point($server) . 'Authorize?wsdl',
      'Operations' => $this->get_end_point($server) . 'Operations?wsdl',
      'PaymentMethods' => $this->get_end_point($server) . 'PaymentMethods?wsdl',
    );
  }

  /**
   * Returns an array with the accepted currencies
   *
   * @return
   *   An array with the ISO codes from the accepted currencies
   */
  public static function get_currencies() {
    return array(
      'ARS',
    );
  }

  /**
   * Returns a currency numeric code, as used by Todo Pago
   *
   * @param $currency_code
   *   Currency code to convert
   *
   * @return
   *   The numeric code of the currency
   */
  public static function currency_code($currency_code) {
    if ($currency = commerce_currency_load($currency_code)) {
      return $currency['numeric_code'];
    }
    return FALSE;
  }

  /**
   * Formats a price amount into a decimal value as expected by Todo Pago.
   *
   * @param $amount
   *   An integer price amount.
   * @param $currency_code
   *   The currency code of the price.
   *
   * @return
   *   The decimal price amount as expected by Todo Pago's API servers.
   */
  public static function format_price_amount($amount, $currency_code) {
    $rounded_amount = commerce_currency_round($amount, commerce_currency_load($currency_code));
    return number_format(commerce_currency_amount_to_decimal($rounded_amount, $currency_code), 2, '.', '');
  }

  /**
   * Returns an array with the Drupal form for the redirection to Todo Pago
   *
   * @param $order
   *   Commerce Order
   *
   * @return
   *   An array with the form
   */
  public function create_order_form($order) {
    $form = array();

    $wrapper = entity_metadata_wrapper('commerce_order', $order);

    $currency_code = $this->settings['currency_code'];
    $order_currency_code = $wrapper->commerce_order_total->currency_code->value();

    if (
      (!empty($settings['allow_supported_currencies'])) &&
      (in_array($order_currency_code, self::get_currencies()))
    ) {
      $currency_code = $order_currency_code;
    }

    $operation_id = uniqid($wrapper->order_number->value() . '_');
    $amount = $this->format_price_amount(commerce_currency_convert($wrapper->commerce_order_total->amount->value(), $order_currency_code, $currency_code), $currency_code);
    $customer_email = $wrapper->mail->value();

    $todopago_info = array(
      'commerce' => array(
        'Security' => $this->settings['security'],
        'EncodingMethod' => 'XML',
        'Merchant' => $this->settings['merchant'],
        'URL_OK' => url('checkout/' . $order->order_id . '/payment/return/' . $order->data['payment_redirect_key'], array('absolute' => TRUE)),
        'URL_ERROR' => url('checkout/' . $order->order_id . '/payment/return/' . $order->data['payment_redirect_key'], array('absolute' => TRUE)),
      ),
      'order' => array(
        'MERCHANT' => $this->settings['merchant'],
        'OPERATIONID' => $operation_id,
        'CURRENCYCODE' => self::currency_code($currency_code),
        'AMOUNT' => $amount,
        'EMAILCLIENTE' => $customer_email,

    'CSBTCITY'=>'CABA', //Ciudad de facturación, MANDATORIO.
    'CSBTCOUNTRY'=>'AR', //País de facturación. MANDATORIO. Código ISO. (http://apps.cybersource.com/library/documentation/sbc/quickref/countries_alpha_list.pdf)
    'CSBTCUSTOMERID'=>$GLOBALS['user']->uid, //Identificador del usuario al que se le emite la factura. MANDATORIO. No puede contener un correo electrónico.
    'CSBTIPADDRESS'=>$_SERVER['REMOTE_ADDRESS'], //IP de la PC del comprador. MANDATORIO.
    'CSBTEMAIL'=>$customer_email, //Mail del usuario al que se le emite la factura. MANDATORIO.
    'CSBTFIRSTNAME'=>'Cristian' ,//Nombre del usuario al que se le emite la factura. MANDATORIO.
    'CSBTLASTNAME'=>'Segarra', //Apellido del usuario al que se le emite la factura. MANDATORIO.
    'CSBTPHONENUMBER'=>'541148554390', //Teléfono del usuario al que se le emite la factura. No utilizar guiones, puntos o espacios. Incluir código de país. MANDATORIO.
    'CSBTPOSTALCODE'=>' C1010AAP', //Código Postal de la dirección de facturación. MANDATORIO.
    'CSBTSTATE'=>'B', //Provincia de la dirección de facturación. MANDATORIO. Ver tabla anexa de provincias.
    'CSBTSTREET1'=>'Cerrito 740', //Domicilio de facturación (calle y nro). MANDATORIO.
    'CSBTSTREET2'=>'Piso 8', //Complemento del domicilio. (piso, departamento). NO MANDATORIO.
    'CSPTCURRENCY'=>'ARS', //Moneda. MANDATORIO.
    'CSPTGRANDTOTALAMOUNT'=>$amount, //Con decimales opcional usando el puntos como separador de decimales. No se permiten comas, ni como separador de miles ni como separador de decimales. MANDATORIO. (Ejemplos:$125,38-> 125.38 $12-> 12 o 12.00)
    'CSMDD6'=>'Web', // Canal de venta. NO MANDATORIO. (Valores posibles: Web, Mobile, Telefonica)
    'CSMDD7'=>'', // Fecha registro comprador(num Dias). NO MANDATORIO.
    'CSMDD8'=>'N', //Usuario Guest? (Y/N). En caso de ser Y, el campo CSMDD9 no deberá enviarse. NO MANDATORIO.
    'CSMDD9'=>'', //Customer password Hash: criptograma asociado al password del comprador final. NO MANDATORIO.
    'CSMDD10'=>'', //Histórica de compras del comprador (Num transacciones). NO MANDATORIO.
    'CSMDD11'=>'', //Customer Cell Phone. NO MANDATORIO.

    'STCITY'=>'CABA', //Ciudad de enví­o de la orden. MANDATORIO
    'STCOUNTRY'=>'AR', //País de envío de la orden. MANDATORIO.
    'STEMAIL'=>$customer_email, //Mail del destinatario, MANDATORIO.
    'STFIRSTNAME'=>'Cristian', //Nombre del destinatario. MANDATORIO.
    'STLASTNAME'=>'Segarra', //Apellido del destinatario. MANDATORIO.
    'STPHONENUMBER'=>'541148554390', //Número de teléfono del destinatario. MANDATORIO.
    'STPOSTALCODE'=>'1414', //Código postal del domicilio de envío. MANDATORIO.
    'STSTATE'=>'D', //Provincia de envío. MANDATORIO. Son de 1 caracter
    'STSTREET1'=>'San Martín 123', //Domicilio de envío. MANDATORIO.
    'STSTREET2'=>'San Luis', //Localidad de envío. NO MANDATORIO.
    'CSMDD12'=>'',//Shipping DeadLine (Num Dias). NO MADATORIO.
    'CSMDD13'=>'',//Método de Despacho. NO MANDATORIO
    'CSMDD14'=>'',//Customer requires Tax Bill ? (Y/N). NO MANDATORIO.
    'CSMDD15'=>'',//Customer Loyality Number. NO MANDATORIO.
    'CSMDD16'=>'',//Promotional / Coupon Code. NO MANDATORIO.
    //Retail: datos a enviar por cada producto, los valores deben estar separado con #:
    'CSITPRODUCTCODE'=>'electronic_good', //Código de producto. CONDICIONAL. Valores posibles(adult_content;coupon;default;electronic_good;electronic_software;gift_certificate;handling_only;service;shipping_and_handling;shipping_only;subscription)
    'CSITPRODUCTDESCRIPTION'=>'NOTEBOOK L845 SP4304LA DF TOSHIBA', //Descripción del producto. CONDICIONAL.
    'CSITPRODUCTNAME'=>'NOTEBOOK L845 SP4304LA DF TOSHIBA', //Nombre del producto. CONDICIONAL.
    'CSITPRODUCTSKU'=>'LEVJNSL36GN', //Código identificador del producto. CONDICIONAL.
    'CSITTOTALAMOUNT'=>$amount, //CSITTOTALAMOUNT=CSITUNITPRICE*CSITQUANTITY "999999[.CC]" Con decimales opcional usando el puntos como separador de decimales. No se permiten comas, ni como separador de miles ni como separador de decimales. CONDICIONAL.
    'CSITQUANTITY'=>'1', //Cantidad del producto. CONDICIONAL.
    'CSITUNITPRICE'=>$amount, //Formato Idem CSITTOTALAMOUNT. CONDICIONAL.

      ),
    );

    $authorize_request = $this->todopago->sendAuthorizeRequest($todopago_info['commerce'], $todopago_info['order']);

    if (
      (empty($authorize_request)) ||
      (empty($authorize_request['StatusCode'])) ||
      (empty($authorize_request['URL_Request'])) ||
      (empty($authorize_request['RequestKey'])) ||
      ($authorize_request['StatusCode'] != -1)
    ) {
      throw new Exception(t('There was a problem connecting with Todo Pago. Try again later.'));
    }

    $transaction = commerce_payment_transaction_new('commerce_todopago', $wrapper->order_number->value());
    $transaction->instance_id = ((!empty($this->settings['instance_id'])) ? ($this->settings['instance_id']) : (''));
    $transaction->remote_id = $authorize_request['RequestKey'];
    $transaction->amount = commerce_currency_decimal_to_amount($todopago_info['order']['AMOUNT'], $currency_code);
    $transaction->currency_code = $currency_code;
    $transaction->status = COMMERCE_PAYMENT_STATUS_PENDING;
    $transaction->data = array(
      'order' => $todopago_info['order'],
      'authorize_request' => $authorize_request,
    );

    commerce_payment_transaction_save($transaction);

    $form['#action'] = $authorize_request['URL_Request'];
    $form['submit'] = array(
      '#type' => 'submit',
      '#value' => t('Proceed to Todo Pago'),
    );

    return($form);
  }

  /**
   * Update the status of a transaction
   *
   * @param $transaction
   *   The transaction who's payment status will be updated
   *
   * @return
   *   void
   */
  public function update_status(&$transaction) {
    //loguear en commerce_todopago_queries
    $pp= $this->todopago->getStatus(array('MERCHANT'=>$this->settings['merchant'], 'OPERATIONID'=>$transaction->data['order']['OPERATIONID']));
    drupal_set_message('getStatus');
    drupal_set_message(print_r($pp, true));

    $optionsGAA = array(
      'Security' => $this->settings['security'],
      'Merchant'  => $this->settings['merchant'],
      'RequestKey' => $transaction->remote_id,
      'AnswerKey' => $transaction->remote_id,
    );

    $pp1 = $this->todopago->getAuthorizeAnswer($optionsGAA);
    drupal_set_message('getAuthorizeAnswer');
    drupal_set_message(print_r($pp1, true));
  }
}


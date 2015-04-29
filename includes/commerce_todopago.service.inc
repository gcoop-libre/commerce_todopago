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
   *   Wrapper of the Commerce Order
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

    $todopago_info = array(
      'commerce' => array(
        'Security' => $this->settings['security'],
        'EncodingMethod' => 'XML',
        'Merchant' => $this->settings['merchant'],
        'URL_OK' => url('checkout/' . $order->order_id . '/payment/return/' . $order->data['payment_redirect_key'], array('absolute' => TRUE)),
        'URL_ERROR' => url('checkout/' . $order->order_id . '/payment/back/' . $order->data['payment_redirect_key'], array('absolute' => TRUE)),
      ),
      'order' => array(
        'MERCHANT' => $this->settings['merchant'],
        'OPERATIONID' => uniqid($wrapper->order_number->value() . '_'),
        'CURRENCYCODE' => self::currency_code($currency_code),
        'AMOUNT' => $this->format_price_amount(commerce_currency_convert($wrapper->commerce_order_total->amount->value(), $order_currency_code, $currency_code), $currency_code),
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
    $transaction->instance_id = $this->settings['instance_id'];
    $transaction->remote_id = $authorize_request['RequestKey'];
    $transaction->amount = commerce_currency_decimal_to_amount($todopago_info['order']['AMOUNT'], $currency_code);
    $transaction->currency_code = $currency_code;
    $transaction->status = COMMERCE_PAYMENT_STATUS_PENDING;
    $transaction->data['order'] = $todopago_info['order'];

    commerce_payment_transaction_save($transaction);

    $form['#action'] = $authorize_request['URL_Request'];
    $form['submit'] = array(
      '#type' => 'submit',
      '#value' => t('Proceed to Todo Pago'),
    );

    return($form);
  }
}


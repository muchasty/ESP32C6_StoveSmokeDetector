const fz = require('zigbee-herdsman-converters/converters/fromZigbee');
const tz = require('zigbee-herdsman-converters/converters/toZigbee');
const exposes = require('zigbee-herdsman-converters/lib/exposes');

const definition = {
    zigbeeModel: ['XIAO-C6-StoveSmoke'],
    model: 'XIAO-C6-StoveSmoke',
    vendor: 'Seeed',
    description: 'Monitor Kuchni (Płomień + SGP40 + SHTC3) z regulowanym progiem',
    fromZigbee: [
        fz.temperature, 
        fz.humidity,    
        {
            cluster: 'msIlluminanceMeasurement',
            type: ['attributeReport', 'readResponse'],
            convert: (model, msg, publish, options, meta) => {
                const ep = msg.endpoint.ID;
                if (msg.data.hasOwnProperty('measuredValue')) {
                    const value = msg.data['measuredValue'];
                    if (ep === 1) return {flame_raw: value};
                    if (ep === 2) return {digital_nose_raw: value};
                }
            },
        },
        {
            cluster: 'genBinaryInput',
            type: ['attributeReport', 'readResponse'],
            convert: (model, msg, publish, options, meta) => {
                const ep = msg.endpoint.ID;
                if (msg.data.hasOwnProperty('presentValue')) {
                    const state = msg.data['presentValue'] ? 'ON' : 'OFF';
                    if (ep === 3) return {flame_status: state};
                    if (ep === 4) return {digital_nose_status: state};
                }
            },
        },
        {
            cluster: 'genMultistateInput',
            type: ['attributeReport', 'readResponse'],
            convert: (model, msg, publish, options, meta) => {
                if (msg.data.hasOwnProperty('presentValue')) {
                    return {sgp_threshold: msg.data['presentValue']};
                }
            },
        },
    ],
    toZigbee: [
        {
            key: ['sgp_threshold'],
            convertSet: async (entity, key, value, meta) => {
                const endpoint = meta.device.getEndpoint(5);
                // Wysyłamy do klastra Analog Output (0x000d), atrybut presentValue (0x0055)
                // Typ danych to Float32 (0x39)
                await endpoint.write('genAnalogOutput', {presentValue: parseFloat(value)}, {manufacturerCode: null});
                return {state: {sgp_threshold: value}};
            },
        },
    ],
    exposes: [
        exposes.numeric('flame_raw', exposes.access.STATE)
            .withUnit('raw')
            .withLabel('Płomień (Analog)')
            .withDescription('Wartości poniżej 2000 mogą świadczyć o wykryciu ognia'),
            
        exposes.numeric('digital_nose_raw', exposes.access.STATE)
            .withUnit('raw')
            .withLabel('SGP40 (Raw)')
            .withDescription('Wartości poniżej 20000 mogą świadczyć o wykryciu przypelanie i lotnych związków.'),
            
        exposes.binary('flame_status', exposes.access.STATE, 'ON', 'OFF')
            .withLabel('Alarm Płomienia')
            .withDescription('Status na podstawie pinu cyfrowego sensora płomienia'),
            
        exposes.binary('digital_nose_status', exposes.access.STATE, 'ON', 'OFF')
            .withLabel('Alarm SGP (Dym/Zapach)')
            .withDescription('Status alarmu na podstawie ustawionego progu'),

        exposes.numeric('sgp_threshold', exposes.access.ALL)
            .withValueMin(0).withValueMax(60000).withUnit('raw')
            .withLabel('Próg Alarmu SGP')
            .withDescription('Domyślna wartość 20000.'),

        // Dodane exposes dla SHTC3 na EP6
        exposes.numeric('temperature_ep6', exposes.access.STATE)
            .withUnit('°C')
            .withLabel('Temperatura')
            .withDescription('Temperatura z sensora SHTC3'),
        exposes.numeric('humidity_ep6', exposes.access.STATE)
            .withUnit('%')
            .withLabel('Wilgotność')
            .withDescription('Wilgotność z sensora SHTC3'),
    ],
    endpoint: (device) => {
        return {ep1: 1, ep2: 2, ep3: 3, ep4: 4, ep5: 5, ep6: 6};
    },
    meta: {multiEndpoint: true},
    configure: async (device, coordinatorEndpoint, logger) => {
        await device.getEndpoint(1).bind('msIlluminanceMeasurement', coordinatorEndpoint);
        await device.getEndpoint(2).bind('msIlluminanceMeasurement', coordinatorEndpoint);
        await device.getEndpoint(3).bind('genBinaryInput', coordinatorEndpoint);
        await device.getEndpoint(4).bind('genBinaryInput', coordinatorEndpoint);
        await device.getEndpoint(5).bind('genMultistateInput', coordinatorEndpoint);

        // Bind dla nowego połączonego endpointu EP6
        const endpoint6 = device.getEndpoint(6);
        await endpoint6.bind('msTemperatureMeasurement', coordinatorEndpoint);
        await endpoint6.bind('msRelativeHumidity', coordinatorEndpoint);
    },
};

module.exports = definition;
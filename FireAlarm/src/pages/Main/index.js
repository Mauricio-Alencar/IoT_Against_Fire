/** eslint disable */
import React, {useState, useRef, useEffect} from 'react';
import {
  View,
  Image,
  Animated,
  StatusBar,
  Dimensions,
  Platform,
  TouchableOpacity,
} from 'react-native';
import {Container, Button, Text, Body, Icon} from 'native-base';
import {colors} from '~/styles';
import Torch from 'react-native-torch';
import MQTT from 'sp-react-native-mqtt';
import Sound from 'react-native-sound';
// import { Container } from './styles';

function Main({navigation}) {
  const [animatedA] = useState(new Animated.Value(0));
  const [opacityA] = useState(new Animated.Value(1));
  const [animatedB] = useState(new Animated.Value(0));
  const [opacityB] = useState(new Animated.Value(1));
  const [isTorchOn, setTorch] = useState(false);
  const [teste, setTeste] = useState(false);
  const [message, setMessage] = useState(false);

  useEffect(() => {
    MQTT.createClient({
      uri: 'mqtt://54.149.157.218:1883',
      clientId: 'clark_123',
      user: 'admin',
      pass: '123',
      auth: true,
      keepalive: 1000000000000,
    })
      .then(function(client) {
        client.on('closed', function() {
          console.log('mqtt.event.closed');
        });

        client.on('error', function(msg) {
          console.log('mqtt.event.error', msg);
        });

        client.on('message', function(msg) {
          if (msg) {
            setMessage(true);
          } else {
            setMessage(false);
            setTorch(false);
          }
        });
        client.on('connect', function() {
          console.log('connected');
          client.subscribe('alarme', 0);
          client.publish('teste/led', 'test', 0, false);
        });
        client.connect();
      })
      .catch(function(err) {
        console.log(err);
      });
  });

  useEffect(() => {
    navigation.addListener('didFocus', () => {
      navigation.setParams(message);
    });
  }, [message, navigation]);
  useEffect(() => {
    if (message) {
      setTimeout(() => {
        Torch.switchState(isTorchOn), setTorch(!isTorchOn);
      }, 2.5);
    }
    if (!message) {
      setTimeout(() => {
        setTorch(false);
      }, 1.5);
    }
  }, [isTorchOn, message]);
  useEffect(() => {
    Animated.stagger(10, [
      Animated.loop(
        Animated.parallel([
          Animated.timing(animatedA, {
            toValue: 3,
            duration: message ? 500 : 2500,
          }),
          Animated.timing(opacityA, {
            toValue: 0,
            duration: message ? 1500 : 5000,
          }),
        ]),
      ),
      Animated.loop(
        Animated.parallel([
          Animated.timing(animatedB, {
            toValue: 3,
            duration: message ? 500 : 2500,
          }),
          Animated.timing(opacityB, {
            toValue: 0,
            duration: message ? 1500 : 5000,
          }),
        ]),
      ),
    ]).start();
  }, [animatedA, animatedB, message, opacityA, opacityB]);

  useEffect(() => {
    if (message) {
      const sound = new Sound(
        // 'http://www.adapticom1.net/adapticom/SoundEffects/nralarm.wav',
        'http://www.adapticom1.net/adapticom/SoundEffects/Star_Trek-Red_Alert.wav',
        null,
        error => {
          setTimeout(() => {
            sound.play();
          }, 2.5),
            setTimeout(() => {
              setTeste(!teste);
            }, 1);
        },
      );
    }
  }, [message, teste]);

  return (
    <Container
      style={{
        flex: 1,
        backgroundColor: message ? colors.primary : colors.secundary,
      }}>
      <StatusBar
        backgroundColor={message ? colors.primary : colors.secundary}
        barStyle="light-content"
      />
      <TouchableOpacity
        onPress={() => {
          setMessage(false),
            setTimeout(() => {
              setTorch(false);
            }, 2);
        }}
        style={{
          flex: 0.5,
          alignContent: 'center',
          justifyContent: 'center',
          alignItens: 'center',
        }}>
        <Animated.View
          style={{
            width: 100,
            height: 100,
            borderRadius: 50,
            backgroundColor: 'white',
            opacity: opacityA,
            alignSelf: 'center',
            transform: [
              {
                scale: animatedA,
              },
            ],
          }}>
          <Animated.View
            style={{
              width: 100,
              height: 100,
              borderRadius: 50,
              backgroundColor: 'white',
              alignSelf: 'center',
              opacity: opacityB,
              transform: [
                {
                  scale: animatedB,
                },
              ],
            }}
          />
        </Animated.View>
        <Button
          transparent
          style={{
            alignSelf: 'center',
            justifyContent: 'flex-start',
            position: 'absolute',
            top:
              Platform.OS == 'android'
                ? Dimensions.get('window').width - 220
                : Dimensions.get('window').width - 260,
          }}>
          <Body>
            <Image
              source={
                message
                  ? require('~/assets/alarm.png')
                  : require('~/assets/smiling.png')
              }
              style={{width: 50, height: 50}}
            />
          </Body>
        </Button>
      </TouchableOpacity>
      <View
        style={{
          flex: 0.5,
          alignContent: 'center',
          justifyContent: 'center',
          alignItens: 'center',
          padding: 20,
        }}>
        {message ? (
          <Text
            style={{textAlign: 'center', fontWeight: 'bold', color: 'white'}}
            numberOfLines={3}>
            Um ALARME foi disparado na sua região, matenha calma, o corpo de
            bombeiros já foi acionado.
          </Text>
        ) : (
          <Text
            style={{textAlign: 'center', fontWeight: 'bold', color: 'white'}}
            numberOfLines={3}>
            Nenhum alarme disparado em sua região, você está seguro =)
          </Text>
        )}
      </View>
    </Container>
  );
}

Main.navigationOptions = ({navigation}) => ({
  headerStyle: {
    backgroundColor: navigation.getParam('message')
      ? colors.primary
      : colors.secundary,
    elevation: 0,
    borderBottomWidth: 0,
  },
  headerLeft: (
    <Button transparent>
      <Icon
        name="settings"
        type="SimpleLineIcons"
        color="white"
        style={{fontSize: 25, color: 'white'}}
      />
    </Button>
  ),
  headerRight: (
    <Button transparent onPress={() => alert('teste')}>
      <Icon
        name="bell-outline"
        type="MaterialCommunityIcons"
        color="white"
        style={{fontSize: 25, color: 'white'}}
      />
    </Button>
  ),
});

export default Main;

import React, {useState} from 'react';
import {View, FlatList, Image, StatusBar} from 'react-native';
import {
  Container,
  Card,
  CardItem,
  Thumbnail,
  Body,
  Left,
  Right,
  Text,
  Button,
  Icon,
} from 'native-base';
import {colors} from '~/styles/index';

// import { Container } from './styles';

function Timeline() {
  const [data, setData] = useState([
    {
      id: 1,
      time: '09:00',
      title: 'Event 1',
      description:
        'Existe um foco de incendio aqui proximo ao 3 Coroas, na pajuçara...',
      uri: '',
      username: 'Anonimo',
      usernameUri:
        'https://d1bvpoagx8hqbg.cloudfront.net/259/eb0a9acaa2c314784949cc8772ca01b3.jpg',
      long: '',
      lat: '',
    },
    {
      id: 2,
      time: '10:45',
      title: 'Event 2',
      description: 'Fogo na AL 101, sentido são miguel dos campos',
      uri:
        'http://www.l12.com.br/hd-imagens/arquivos/10686848_413062878841219_2840130213844052441_n-11.jpg',
      username: 'Matheus',
      usernameUri:
        'https://rodrigogavini.files.wordpress.com/2009/11/seu-ze-de-lora1-menor1.jpg?w=676',
      long: '',
      lat: '',
    },
    {
      id: 3,
      time: '12:00',
      title: 'Event 3',
      description:
        'Povo aqui na rua ta queimando lixooo, alguem faz alguma coisa',
      uri: '',
      username: 'Seu Zé',
      usernameUri:
        'https://rodrigogavini.files.wordpress.com/2009/11/seu-ze-de-lora1-menor1.jpg?w=676',
      long: '',
      lat: '',
    },
    {
      id: 4,
      time: '14:00',
      title: 'Event 4',
      description: 'Brazilia Pegando Fogo, Oloko Bixo',
      uri:
        'https://s2.glbimg.com/VHJvixWuqbkezNOhIDaHDnQvHC8=/0x0:1280x720/984x0/smart/filters:strip_icc()/i.s3.glbimg.com/v1/AUTH_59edd422c0c84a879bd37670ae4f538a/internal_photos/bs/2019/h/v/MKV4p0S8iAHBOmiYEIww/carro-1.jpg',
      username: 'Anonimo',
      usernameUri:
        'https://d1bvpoagx8hqbg.cloudfront.net/259/eb0a9acaa2c314784949cc8772ca01b3.jpg',
      long: '',
      lat: '',
    },
    {
      id: 5,
      time: '16:30',
      description:
        'Incendio no Apartamento 202, coral das ostras, pajucara maceió!',
      uri: `http://s2.glbimg.com/RZvh1fi_7HudsYatZ2yDSIs_988xnrbp_0SWggZTbqBIoz-HdGixxa_8qOZvMp3w/
      s.glbimg.com/jo/g1/f/original/2012/09/06/roberto_1.jpg`,
      username: 'Dona Maria',
      usernameUri:
        'https://s2.glbimg.com/cqClxS9c_OQ1S0qretA6uTDTzeE=/0x0:1040x580/2000x0/smart/filters:strip_icc()/i.s3.glbimg.com/v1/AUTH_b58693ed41d04a39826739159bf600a0/internal_photos/bs/2019/i/V/0tmIdISqa9M23MpMszVQ/2.png',
      long: '',
      lat: '',
    },
  ]);
  return (
    <Container>
      <StatusBar backgroundColor={colors.primary} barStyle="light-content" />
      <FlatList
        data={data}
        style={{backgroudColor: colors.primary}}
        renderItem={({item}) => (
          <Card style={{margin: 40}}>
            <CardItem>
              <Left>
                <Thumbnail source={{uri: item.usernameUri}} />
                <Body>
                  <Text>{item.username}</Text>
                  <Text note>{item.time}</Text>
                </Body>
              </Left>
            </CardItem>
            <CardItem>
              <Text>{item.description}</Text>
            </CardItem>
            <CardItem cardBody>
              <Body>
                {item.uri ? (
                  <Image
                    source={{uri: item.uri}}
                    style={{height: 200, width: 500, flex: 1}}
                  />
                ) : null}
              </Body>
            </CardItem>
          </Card>
        )}
      />
    </Container>
  );
}

Timeline.navigationOptions = ({}) => ({
  title: 'Alertas',
  headerStyle: {
    backgroundColor: colors.primary,
    elevation: 0,
    borderBottomWidth: 0,
  },
  headerTintColor: '#fff',
  headerTitleStyle: {
    fontWeight: 'bold',
  },
  headerLeft: (
    <Button transparent>
      <Icon
        name="back"
        type="AntDesign"
        color="white"
        style={{fontSize: 25, color: 'white'}}
      />
    </Button>
  ),
});

export default Timeline;

import {createAppContainer, createSwitchNavigator} from 'react-navigation';
import {createStackNavigator} from 'react-navigation-stack';
import Main from '~/pages/Main';
import TimeLine from '~/pages/Timeline';

const Routes = createAppContainer(
  createStackNavigator({
    Main,
    // TimeLine,
  }),
);

export default Routes;

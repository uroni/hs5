import logoImage from '../assets/hs5.png'
import { useSnapshot } from 'valtio';
import { state } from '../App';
import { useStackStyles } from './StackStyles';
import { Avatar, Image } from '@fluentui/react-components';

export const HeaderBar = () => {

  const snap = useSnapshot(state);
  const styles = useStackStyles();

  return (
    <div className={styles.stackHorizontal} style={{alignItems: "center"}}>
        <div className={styles.item} style={{padding: "5px"}}>
          <Image src={logoImage} fit='contain' />
        </div>
        <div className={styles.item}>
          HS5
        </div>
        <div className={styles.itemGrow}>

        </div>
        <div className={styles.item}>
          <Avatar aria-label='guest' />
        </div>
      </div>
  );
}

export default HeaderBar;